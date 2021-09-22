#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

struct Config {
	char title[512];
	size_t width;
	size_t height;
	bool debug;
};

#endif
