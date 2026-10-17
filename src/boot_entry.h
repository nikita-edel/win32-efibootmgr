#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define LOAD_OPT_ACTIVE 0x1

typedef struct {
	uint32_t attributes;
	uint16_t dp_len;
	wchar_t* description;
	uint8_t* dp_data;
	uint8_t* optional_data;
	uint32_t optional_data_len;
} boot_entry_t;

bool boot_entry_parse(boot_entry_t* e, const uint8_t* data, uint32_t size);
uint8_t* boot_entry_serialize(const boot_entry_t* e, uint32_t* out_size);
void boot_entry_free(boot_entry_t* e);
