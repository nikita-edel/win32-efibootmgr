#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "boot_entry.h"
#include "dp.h"
#include "efi_vars.h"

static void winerr(const wchar_t* ctx) {
	DWORD e = GetLastError();
	wchar_t msg[256] = { 0 };
	size_t len;

	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				   NULL, e, 0, msg, 256, NULL);
	len = wcslen(msg);
	while (len > 0 && (msg[len - 1] == L'\r' || msg[len - 1] == L'\n'))
		msg[--len] = 0;
	fwprintf(stderr, L"error: %ls: %ls (0x%08X)\n", ctx, msg, (unsigned)e);
}

static void bootname(uint16_t num, wchar_t buf[9]) {
	swprintf(buf, 9, L"Boot%04X", num);
}

static bool parse_hex16(const wchar_t* s, uint16_t* out) {
	wchar_t* end;
	unsigned long v = wcstoul(s, &end, 16);

	if (end == s) return false;
	*out = (uint16_t)v;
	return true;
}

static bool get_u16(const wchar_t* name, uint16_t* out) {
	return efi_get_var(name, out, 2, NULL) == 2;
}

static uint16_t* read_bootorder(uint32_t* count) {
	static uint8_t raw[EFI_VAR_MAX_SIZE];
	uint32_t n = efi_get_var(L"BootOrder", raw, sizeof(raw), NULL);
	uint16_t* order;

	*count = 0;
	if (n == 0 || n % 2) return NULL;
	order = malloc(n);
	if (!order) return NULL;
	memcpy(order, raw, n);
	*count = n / 2;
	return order;
}

static void print_list(const uint16_t* nums, uint32_t count) {
	for (uint32_t i = 0; i < count; i++)
		wprintf(i ? L",%04X" : L"%04X", nums[i]);
	wprintf(L"\n");
}

static void print_entry(uint16_t num, bool verbose) {
	static uint8_t raw[EFI_VAR_MAX_SIZE];
	wchar_t name[9];
	boot_entry_t e;
	uint32_t n;

	bootname(num, name);
	n = efi_get_var(name, raw, sizeof(raw), NULL);
	if (n == 0) return;
	if (!boot_entry_parse(&e, raw, n)) {
		wprintf(L"%ls  (parse error)\n", name);
		return;
	}

	wprintf(L"%ls%lc %ls", name, (e.attributes & LOAD_OPT_ACTIVE) ? L'*' : L' ',
			e.description[0] ? e.description : L"(no description)");
	if (verbose && e.dp_len) {
		wprintf(L"\t");
		dp_print(e.dp_data, e.dp_len);
	}
	wprintf(L"\n");
	boot_entry_free(&e);
}

static void cmd_print(bool verbose) {
	uint32_t count;
	uint16_t* order = read_bootorder(&count);
	uint16_t v;

	if (get_u16(L"BootCurrent", &v)) wprintf(L"BootCurrent: %04X\n", v);
	if (get_u16(L"Timeout", &v)) wprintf(L"Timeout: %u seconds\n", v);
	if (count) {
		wprintf(L"BootOrder: ");
		print_list(order, count);
	}
	if (get_u16(L"BootNext", &v)) wprintf(L"BootNext: %04X\n", v);
	wprintf(L"\n");

	for (uint32_t i = 0; i < count; i++) print_entry(order[i], verbose);

	for (uint16_t num = 0; num <= 0xff; num++) {
		uint32_t i = 0;
		while (i < count && order[i] != num) i++;
		if (i == count) print_entry(num, verbose);
	}
	free(order);
}

static bool cmd_del_var(const wchar_t* name) {
	if (!efi_del_var(name)) {
		winerr(name);
		return false;
	}
	wprintf(L"Deleted %ls\n", name);
	return true;
}

static bool cmd_set_u16(const wchar_t* name, uint16_t v) {
	if (!efi_set_var(name, &v, 2, EFI_VAR_DEFAULT)) {
		winerr(name);
		return false;
	}
	return true;
}

static bool cmd_set_bootorder(const wchar_t* s) {
	uint16_t nums[512];
	uint32_t count = 0;
	wchar_t* end;

	while (*s && count < 512) {
		unsigned long v = wcstoul(s, &end, 16);
		if (end == s) break;
		nums[count++] = (uint16_t)v;
		s = *end == L',' ? end + 1 : end;
	}
	if (count == 0) {
		fwprintf(stderr, L"error: no valid hex numbers in bootorder\n");
		return false;
	}
	if (!efi_set_var(L"BootOrder", nums, count * 2, EFI_VAR_DEFAULT)) {
		winerr(L"BootOrder");
		return false;
	}
	wprintf(L"BootOrder set: ");
	print_list(nums, count);
	return true;
}

static bool cmd_set_bootnext(const wchar_t* s) {
	uint16_t num;

	if (!parse_hex16(s, &num)) {
		fwprintf(stderr, L"error: invalid boot number '%ls'\n", s);
		return false;
	}
	if (!cmd_set_u16(L"BootNext", num)) return false;
	wprintf(L"BootNext: %04X\n", num);
	return true;
}

static bool cmd_set_timeout(const wchar_t* s) {
	wchar_t* end;
	unsigned long v = wcstoul(s, &end, 10);

	if (end == s || v > 0xffff) {
		fwprintf(stderr, L"error: invalid timeout '%ls' (0-65535)\n", s);
		return false;
	}
	if (!cmd_set_u16(L"Timeout", (uint16_t)v)) return false;
	wprintf(L"Timeout: %lu seconds\n", v);
	return true;
}

static bool cmd_delete(uint16_t num) {
	wchar_t name[9];
	uint32_t count, kept = 0;
	uint16_t* order = read_bootorder(&count);

	for (uint32_t i = 0; i < count; i++)
		if (order[i] != num) order[kept++] = order[i];
	if (kept < count) {
		if (kept)
			efi_set_var(L"BootOrder", order, kept * 2, EFI_VAR_DEFAULT);
		else
			efi_del_var(L"BootOrder");
	}
	free(order);

	bootname(num, name);
	return cmd_del_var(name);
}

static bool cmd_set_active(uint16_t num, bool active) {
	static uint8_t raw[EFI_VAR_MAX_SIZE];
	wchar_t name[9];
	boot_entry_t e;
	uint32_t n, size;
	uint8_t* data;
	bool ok;

	bootname(num, name);
	n = efi_get_var(name, raw, sizeof(raw), NULL);
	if (n == 0) {
		fwprintf(stderr, L"error: %ls not found\n", name);
		return false;
	}
	if (!boot_entry_parse(&e, raw, n)) {
		fwprintf(stderr, L"error: %ls parse failed\n", name);
		return false;
	}

	if (active)
		e.attributes |= LOAD_OPT_ACTIVE;
	else
		e.attributes &= ~LOAD_OPT_ACTIVE;

	data = boot_entry_serialize(&e, &size);
	boot_entry_free(&e);
	if (!data) return false;
	ok = efi_set_var(name, data, size, EFI_VAR_DEFAULT);
	free(data);
	if (!ok) {
		winerr(name);
		return false;
	}
	wprintf(L"%ls %ls\n", name, active ? L"activated" : L"deactivated");
	return true;
}

static bool cmd_create(const wchar_t* disk, uint32_t part,
					   const wchar_t* loader, const wchar_t* label) {
	static uint8_t raw[EFI_VAR_MAX_SIZE];
	wchar_t name[9];
	boot_entry_t e = { 0 };
	uint32_t num, size, count;
	uint16_t *order, *neworder;
	uint8_t* data;
	bool ok;

	e.attributes = LOAD_OPT_ACTIVE;
	e.description = (wchar_t*)label;
	e.dp_data = dp_build_gpt_file(disk, part, loader, &e.dp_len);
	if (!e.dp_data) {
		fwprintf(stderr,
				 L"error: could not build device path for %ls part %u\n", disk,
				 part);
		winerr(L"dp_build_gpt_file");
		return false;
	}

	for (num = 0; num < 0xffff; num++) {
		bootname((uint16_t)num, name);
		if (efi_get_var(name, raw, sizeof(raw), NULL) == 0) break;
	}
	bootname((uint16_t)num, name);

	data = boot_entry_serialize(&e, &size);
	free(e.dp_data);
	if (!data) return false;
	ok = efi_set_var(name, data, size, EFI_VAR_DEFAULT);
	free(data);
	if (!ok) {
		winerr(name);
		return false;
	}

	order = read_bootorder(&count);
	neworder = malloc((count + 1) * 2);
	if (neworder) {
		uint32_t n = 0;
		neworder[n++] = (uint16_t)num;
		for (uint32_t i = 0; i < count; i++)
			if (order[i] != num) neworder[n++] = order[i];
		efi_set_var(L"BootOrder", neworder, n * 2, EFI_VAR_DEFAULT);
		free(neworder);
	}
	free(order);

	wprintf(L"Created %ls: %ls\n", name, label);
	return true;
}

static void usage(const wchar_t* prog) {
	fwprintf(
		stderr,
		L"Usage: %ls [options]\n"
		L"\n"
		L"  (no options)               Print current EFI boot configuration\n"
		L"  -v, --verbose              Also show device paths\n"
		L"\n"
		L"  -o, --bootorder XXXX,...   Set BootOrder (comma-separated hex)\n"
		L"  -O, --delete-bootorder     Delete BootOrder variable\n"
		L"  -n, --bootnext XXXX        Set BootNext (one-time override)\n"
		L"  -N, --delete-bootnext      Delete BootNext variable\n"
		L"  -t, --timeout SEC          Set firmware boot timeout (0-65535)\n"
		L"  -T, --delete-timeout       Delete Timeout variable\n"
		L"\n"
		L"  -b, --bootnum XXXX         Select a boot entry (required for "
		L"-B/-a/-A)\n"
		L"  -B, --delete               Delete the selected entry\n"
		L"  -a, --active               Mark the selected entry active\n"
		L"  -A, --inactive             Mark the selected entry inactive\n"
		L"\n"
		L"  -c, --create               Create a new boot entry\n"
		L"  -d, --disk PATH            Disk (e.g. \\\\.\\PhysicalDrive0)\n"
		L"  -p, --part N               GPT partition number (1-based)\n"
		L"  -l, --loader PATH          EFI loader path (e.g. "
		L"\\EFI\\ubuntu\\shimx64.efi)\n"
		L"  -L, --label NAME           Boot entry description\n"
		L"\n"
		L"  -h, --help                 Show this help\n"
		L"\n"
		L"Requires Administrator privileges and UEFI firmware (Windows 8+).\n",
		prog);
}

static bool opt(const wchar_t* arg, const wchar_t* s, const wchar_t* l) {
	return !wcscmp(arg, s) || !wcscmp(arg, l);
}

int wmain(int argc, wchar_t* argv[]) {
	bool verbose = false, del_order = false, del_next = false,
		 del_timeout = false;
	bool has_num = false, do_delete = false, do_active = false,
		 do_inactive = false;
	bool do_create = false;
	const wchar_t *order = NULL, *next = NULL, *timeout = NULL;
	const wchar_t *disk = NULL, *loader = NULL, *label = NULL, *s;
	uint32_t part = 0;
	uint16_t num = 0;
	bool any = false;
	int ret = 0;

	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

#define TAKE(dst)                                                            \
	do {                                                                     \
		if (i + 1 >= argc) {                                                 \
			fwprintf(stderr, L"error: %ls requires an argument\n", argv[i]); \
			return 1;                                                        \
		}                                                                    \
		dst = argv[++i];                                                     \
	} while (0)

	for (int i = 1; i < argc; i++) {
		const wchar_t* arg = argv[i];

		if (opt(arg, L"-v", L"--verbose")) {
			verbose = true;
		} else if (opt(arg, L"-o", L"--bootorder")) {
			TAKE(order);
		} else if (opt(arg, L"-O", L"--delete-bootorder")) {
			del_order = true;
		} else if (opt(arg, L"-n", L"--bootnext")) {
			TAKE(next);
		} else if (opt(arg, L"-N", L"--delete-bootnext")) {
			del_next = true;
		} else if (opt(arg, L"-t", L"--timeout")) {
			TAKE(timeout);
		} else if (opt(arg, L"-T", L"--delete-timeout")) {
			del_timeout = true;
		} else if (opt(arg, L"-b", L"--bootnum")) {
			TAKE(s);
			if (!parse_hex16(s, &num)) {
				fwprintf(stderr, L"error: invalid hex boot number '%ls'\n", s);
				return 1;
			}
			has_num = true;
		} else if (opt(arg, L"-B", L"--delete")) {
			do_delete = true;
		} else if (opt(arg, L"-a", L"--active")) {
			do_active = true;
		} else if (opt(arg, L"-A", L"--inactive")) {
			do_inactive = true;
		} else if (opt(arg, L"-c", L"--create")) {
			do_create = true;
		} else if (opt(arg, L"-d", L"--disk")) {
			TAKE(disk);
		} else if (opt(arg, L"-p", L"--part")) {
			wchar_t* end;
			TAKE(s);
			part = wcstoul(s, &end, 10);
			if (end == s || part == 0) {
				fwprintf(stderr, L"error: invalid partition number '%ls'\n", s);
				return 1;
			}
		} else if (opt(arg, L"-l", L"--loader")) {
			TAKE(loader);
		} else if (opt(arg, L"-L", L"--label")) {
			TAKE(label);
		} else if (opt(arg, L"-h", L"--help")) {
			usage(argv[0]);
			return 0;
		} else {
			fwprintf(stderr, L"error: unknown option '%ls'\n", arg);
			usage(argv[0]);
			return 1;
		}
	}
#undef TAKE

	if ((do_delete || do_active || do_inactive) && !has_num) {
		fwprintf(stderr, L"error: -B/-a/-A require -b/--bootnum\n");
		return 1;
	}
	if (do_create && (!disk || !part || !loader || !label)) {
		fwprintf(stderr,
				 L"error: --create requires -d/--disk, -p/--part, -l/--loader, "
				 L"-L/--label\n");
		return 1;
	}
	if (!efi_acquire_privilege()) {
		fwprintf(stderr,
				 L"error: cannot acquire SeSystemEnvironmentPrivilege\n"
				 L"       Run this program as Administrator.\n");
		return 1;
	}

#define RUN(x)             \
	do {                   \
		any = true;        \
		if (!(x)) ret = 1; \
	} while (0)

	if (order) RUN(cmd_set_bootorder(order));
	if (del_order) RUN(cmd_del_var(L"BootOrder"));
	if (next) RUN(cmd_set_bootnext(next));
	if (del_next) RUN(cmd_del_var(L"BootNext"));
	if (timeout) RUN(cmd_set_timeout(timeout));
	if (del_timeout) RUN(cmd_del_var(L"Timeout"));
	if (do_delete) RUN(cmd_delete(num));
	if (do_active) RUN(cmd_set_active(num, true));
	if (do_inactive) RUN(cmd_set_active(num, false));
	if (do_create) RUN(cmd_create(disk, part, loader, label));
#undef RUN

	if (!any) cmd_print(verbose);
	return ret;
}
