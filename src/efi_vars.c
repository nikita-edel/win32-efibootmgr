#include "efi_vars.h"

bool efi_acquire_privilege(void) {
	HANDLE token;
	TOKEN_PRIVILEGES tp;
	bool ok = false;

	if (!OpenProcessToken(GetCurrentProcess(),
						  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return false;

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (LookupPrivilegeValueW(NULL, SE_SYSTEM_ENVIRONMENT_NAME,
							  &tp.Privileges[0].Luid) &&
		AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL))
		ok = GetLastError() == ERROR_SUCCESS;

	CloseHandle(token);
	return ok;
}

uint32_t efi_get_var(const wchar_t* name, void* buf, uint32_t size,
					 uint32_t* attrs) {
	DWORD attr = 0;
	DWORD n = GetFirmwareEnvironmentVariableExW(name, EFI_GLOBAL_GUID, buf,
												size, &attr);
	if (n && attrs) *attrs = attr;
	return n;
}

bool efi_set_var(const wchar_t* name, const void* buf, uint32_t size,
				 uint32_t attrs) {
	return SetFirmwareEnvironmentVariableExW(name, EFI_GLOBAL_GUID, (void*)buf,
											 size, attrs);
}

bool efi_del_var(const wchar_t* name) {
	return SetFirmwareEnvironmentVariableExW(name, EFI_GLOBAL_GUID, NULL, 0, 0);
}
