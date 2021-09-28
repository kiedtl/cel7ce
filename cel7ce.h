#ifndef CEL7CE_H
#define CEL7CE_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "fe.h"

#define PALETTE_START       0x4000
#define FONT_START          0x4040
#define DISPLAY_START       0x52a0
#define FE_CTX_DATA_SIZE    65535
#define FONT_HEIGHT         7
#define FONT_WIDTH          7
#define FONT_FALLBACK_GLYPH 0x7F

#define UNUSED(x) (void)(x)
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

struct Config {
	char title[512];
	size_t width;
	size_t height;
	size_t scale;
	bool debug;
};

struct ApiFunc {
	const char *name;
	fe_Object *(*func)(fe_Context *, fe_Object *);
};

extern struct Config config;

extern uint8_t *memory;
extern size_t memory_sz;
extern size_t color;

extern void *fe_ctx_data;
extern fe_Context *fe_ctx;
extern bool quit;

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture *texture;

extern char font[96 * FONT_HEIGHT][FONT_WIDTH];
extern const struct ApiFunc fe_apis[16];

uint32_t decode_u32_from_bytes(uint8_t *bytes);

#endif
