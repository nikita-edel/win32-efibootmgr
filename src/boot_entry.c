#include "boot_entry.h"

#include <stdlib.h>
#include <string.h>

bool boot_entry_parse(boot_entry_t* e, const uint8_t* data, uint32_t size) {
	const uint8_t *p, *end = data + size;
	size_t len = 0;

	memset(e, 0, sizeof(*e));
	if (size < 6) return false;
	memcpy(&e->attributes, data, 4);
	memcpy(&e->dp_len, data + 4, 2);
	p = data + 6;

	for (;;) {
		if (p + (len + 1) * 2 > end) return false;
		if (((const wchar_t*)p)[len] == 0) break;
		len++;
	}
	len = (len + 1) * 2;
	e->description = malloc(len);
	if (!e->description) return false;
	memcpy(e->description, p, len);
	p += len;

	if ((uint32_t)(end - p) < e->dp_len) goto fail;
	e->dp_data = malloc(e->dp_len ? e->dp_len : 1);
	if (!e->dp_data) goto fail;
	memcpy(e->dp_data, p, e->dp_len);
	p += e->dp_len;

	e->optional_data_len = (uint32_t)(end - p);
	if (e->optional_data_len) {
		e->optional_data = malloc(e->optional_data_len);
		if (!e->optional_data) goto fail;
		memcpy(e->optional_data, p, e->optional_data_len);
	}
	return true;

fail:
	boot_entry_free(e);
	return false;
}

uint8_t* boot_entry_serialize(const boot_entry_t* e, uint32_t* out_size) {
	uint32_t desc = (uint32_t)(wcslen(e->description) + 1) * 2;
	uint32_t total = 6 + desc + e->dp_len + e->optional_data_len;
	uint8_t* buf = malloc(total);
	uint8_t* p = buf;

	if (!buf) return NULL;
	memcpy(p, &e->attributes, 4);
	memcpy(p + 4, &e->dp_len, 2);
	p += 6;
	memcpy(p, e->description, desc);
	p += desc;
	if (e->dp_len) memcpy(p, e->dp_data, e->dp_len);
	p += e->dp_len;
	if (e->optional_data_len) memcpy(p, e->optional_data, e->optional_data_len);

	*out_size = total;
	return buf;
}

void boot_entry_free(boot_entry_t* e) {
	free(e->description);
	free(e->dp_data);
	free(e->optional_data);
	memset(e, 0, sizeof(*e));
}
