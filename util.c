#if defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif

#include <stdint.h>
#include "cel7ce.h"

char *
get_username(void)
{
#if defined(__linux__)
	char *u = getenv("USER");
	if (u == NULL) u = "root";
	return u;
#elif defined(_WIN32) || defined(__WIN32__)
	TCHAR buf[4096];
	DWORD size = ARRAY_LEN(buf);
 
	if(!GetUserName(buf, &size))
  		return "root";
	return buf;
#else
#error TODO: Non Linux/Windows platforms
#endif
}

uint32_t
decode_u32_from_bytes(uint8_t *bytes)
{
	uint32_t accm = 0;
	for (ssize_t b = 3; b >= 0; --b)
		accm = (accm << 8) | bytes[b];
	return accm;
}
