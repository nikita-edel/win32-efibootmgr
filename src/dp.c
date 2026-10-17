#include "dp.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winioctl.h>

#define DP_MEDIA 0x04
#define DP_MEDIA_HD 0x01
#define DP_MEDIA_FILE 0x04
#define DP_END 0x7f
#define DP_END_ENTIRE 0xff

#pragma pack(push, 1)
typedef struct {
	uint8_t type;
	uint8_t subtype;
	uint16_t len;
} dp_hdr_t;

typedef struct {
	dp_hdr_t hdr;
	uint32_t part_num;
	uint64_t part_start;
	uint64_t part_size;
	uint8_t sig[16];
	uint8_t mbr_type;
	uint8_t sig_type;
} dp_hd_t;
#pragma pack(pop)

_Static_assert(sizeof(dp_hd_t) == 42, "dp_hd_t size");

static void print_guid(const uint8_t* g) {
	uint32_t a;
	uint16_t b, c;

	memcpy(&a, g, 4);
	memcpy(&b, g + 4, 2);
	memcpy(&c, g + 6, 2);
	wprintf(L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", a, b, c, g[8],
			g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}

void dp_print(const uint8_t* dp, uint16_t len) {
	const uint8_t *p = dp, *end = dp + len;

	while (p + sizeof(dp_hdr_t) <= end) {
		const dp_hdr_t* h = (const dp_hdr_t*)p;

		if (h->len < sizeof(dp_hdr_t) || p + h->len > end) break;
		if (h->type == DP_END && h->subtype == DP_END_ENTIRE) break;
		if (p != dp) wprintf(L"/");

		if (h->type == DP_MEDIA && h->subtype == DP_MEDIA_HD &&
			h->len >= sizeof(dp_hd_t)) {
			const dp_hd_t* hd = (const dp_hd_t*)p;
			wprintf(L"HD(%u,GPT,", hd->part_num);
			print_guid(hd->sig);
			wprintf(L",%llu,%llu)", (unsigned long long)hd->part_start,
					(unsigned long long)hd->part_size);
		} else if (h->type == DP_MEDIA && h->subtype == DP_MEDIA_FILE &&
				   h->len >= 6) {
			wprintf(L"File(%ls)", (const wchar_t*)(p + sizeof(dp_hdr_t)));
		} else {
			wprintf(L"Type(%02X,%02X)", h->type, h->subtype);
		}
		p += h->len;
	}
}

static bool get_partition(const wchar_t* disk, uint32_t partnum,
						  uint64_t* start, uint64_t* size, GUID* guid) {
	DISK_GEOMETRY_EX geo;
	DRIVE_LAYOUT_INFORMATION_EX* layout;
	DWORD got, layout_size, sector;
	bool found = false;
	HANDLE h;

	h = CreateFileW(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;

	layout_size = sizeof(*layout) + 127 * sizeof(PARTITION_INFORMATION_EX);
	layout = malloc(layout_size);
	if (!layout ||
		!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geo,
						 sizeof(geo), &got, NULL) ||
		!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, layout,
						 layout_size, &got, NULL))
		goto out;

	sector = geo.Geometry.BytesPerSector ? geo.Geometry.BytesPerSector : 512;
	for (DWORD i = 0; i < layout->PartitionCount; i++) {
		PARTITION_INFORMATION_EX* e = &layout->PartitionEntry[i];
		if (e->PartitionStyle != PARTITION_STYLE_GPT ||
			e->PartitionNumber != partnum)
			continue;
		*start = e->StartingOffset.QuadPart / sector;
		*size = e->PartitionLength.QuadPart / sector;
		*guid = e->Gpt.PartitionId;
		found = true;
		break;
	}

out:
	free(layout);
	CloseHandle(h);
	return found;
}

uint8_t* dp_build_gpt_file(const wchar_t* disk, uint32_t partnum,
						   const wchar_t* loader, uint16_t* out_len) {
	uint64_t start, size;
	GUID guid;
	uint32_t path_len, file_len, total;
	uint8_t* buf;
	dp_hd_t* hd;
	dp_hdr_t *file, *end;

	if (!get_partition(disk, partnum, &start, &size, &guid)) return NULL;

	path_len = (uint32_t)(wcslen(loader) + 1) * 2;
	file_len = sizeof(dp_hdr_t) + path_len;
	total = sizeof(dp_hd_t) + file_len + sizeof(dp_hdr_t);
	if (total > 0xffff) return NULL;
	buf = calloc(1, total);
	if (!buf) return NULL;

	hd = (dp_hd_t*)buf;
	hd->hdr.type = DP_MEDIA;
	hd->hdr.subtype = DP_MEDIA_HD;
	hd->hdr.len = sizeof(dp_hd_t);
	hd->part_num = partnum;
	hd->part_start = start;
	hd->part_size = size;
	memcpy(hd->sig, &guid, 16);
	hd->mbr_type = 2; /* GPT */
	hd->sig_type = 2; /* GUID */

	file = (dp_hdr_t*)(buf + sizeof(dp_hd_t));
	file->type = DP_MEDIA;
	file->subtype = DP_MEDIA_FILE;
	file->len = (uint16_t)file_len;
	memcpy(file + 1, loader, path_len);

	end = (dp_hdr_t*)((uint8_t*)file + file_len);
	end->type = DP_END;
	end->subtype = DP_END_ENTIRE;
	end->len = sizeof(dp_hdr_t);

	*out_len = (uint16_t)total;
	return buf;
}
