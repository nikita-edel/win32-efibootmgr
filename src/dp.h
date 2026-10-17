#pragma once
#include <stdint.h>
#include <wchar.h>

void dp_print(const uint8_t* dp, uint16_t len);
uint8_t* dp_build_gpt_file(const wchar_t* disk, uint32_t partnum,
						   const wchar_t* loader, uint16_t* out_len);
