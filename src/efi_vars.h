#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#define EFI_GLOBAL_GUID L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}"

#define EFI_VAR_NV 0x1
#define EFI_VAR_BS 0x2
#define EFI_VAR_RT 0x4
#define EFI_VAR_DEFAULT (EFI_VAR_NV | EFI_VAR_BS | EFI_VAR_RT)

#define EFI_VAR_MAX_SIZE 32768

bool efi_acquire_privilege(void);
uint32_t efi_get_var(const wchar_t* name, void* buf, uint32_t size,
					 uint32_t* attrs);
bool efi_set_var(const wchar_t* name, const void* buf, uint32_t size,
				 uint32_t attrs);
bool efi_del_var(const wchar_t* name);
