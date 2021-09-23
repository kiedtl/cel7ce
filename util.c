#include <stdint.h>
#include "cel7ce.h"

uint32_t
decode_u32_from_bytes(uint8_t *bytes)
{
	uint32_t accm = 0;
	for (ssize_t b = 3; b >= 0; --b)
		accm = (accm << 8) | bytes[b];
	return accm;
}
