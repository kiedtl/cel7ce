#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL.h>
#include "fe.h"

#include "config.h"

#define PALETTE_START    0x4000
#define FONT_START       0x4040
#define DISPLAY_START    0x52a0
#define FE_CTX_DATA_SIZE 65535
#define FONT_HEIGHT      7
#define FONT_WIDTH       7
#define PIXEL_HEIGHT     4
#define PIXEL_WIDTH      4

#define UNUSED(x) (void)(x)
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t colors[] = {
	0x0b0c0d, 0xf7f7e6, 0xf71467, 0xfd971f,
	0xe6d415, 0xa0e01f, 0x46bbff, 0xa98aff,
	0xf9aaaf, 0xab3347, 0x37946e, 0x2a4669,
	0x7c8d99, 0xc2beae, 0x75715e, 0x3e3d32
};

static struct Config config = {
	.title = "cel7 ce",
	.width = 16,
	.height = 16,
	.debug = false,
};

static uint8_t *memory = NULL;
static size_t memory_sz = 0;
static size_t color = 1;

static void *fe_ctx_data = NULL;
static fe_Context *fe_ctx = NULL;
static bool quit = false;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

#include "api.c"
#include "font.c"

static void
init_fe(void)
{
	fe_ctx_data = malloc(FE_CTX_DATA_SIZE);
	fe_ctx = fe_open(fe_ctx_data, FE_CTX_DATA_SIZE);

	for (size_t i = 0; i < ARRAY_LEN(fe_apis); ++i) {
		fe_set(
			fe_ctx,
			fe_symbol(fe_ctx, fe_apis[i].name),
			fe_cfunc(fe_ctx, fe_apis[i].func)
		);
	}

	// Set default values of variables
	{
		fe_Object *objs[3];

		objs[0] = fe_symbol(fe_ctx, "=");
		objs[1] = fe_symbol(fe_ctx, "width");
		objs[2] = fe_number(fe_ctx, config.width);
		fe_eval(fe_ctx, fe_list(fe_ctx, objs, ARRAY_LEN(objs)));

		objs[0] = fe_symbol(fe_ctx, "=");
		objs[1] = fe_symbol(fe_ctx, "height");
		objs[2] = fe_number(fe_ctx, config.height);
		fe_eval(fe_ctx, fe_list(fe_ctx, objs, ARRAY_LEN(objs)));
	}

	int gc = fe_savegc(fe_ctx);

	if (fe_type(fe_ctx, fe_eval(fe_ctx, fe_symbol(fe_ctx, "title"))) == FE_TSTRING) {
		fe_Object *fe_title = fe_eval(fe_ctx, fe_symbol(fe_ctx, "title"));
		fe_tostring(fe_ctx, fe_title, (char *)&config.title, sizeof(config.title));
	}

	if (fe_type(fe_ctx, fe_eval(fe_ctx, fe_symbol(fe_ctx, "width"))) == FE_TNUMBER) {
		fe_Object *fe_width = fe_eval(fe_ctx, fe_symbol(fe_ctx, "width"));
		config.width = (size_t)fe_tonumber(fe_ctx, fe_width);
	}

	if (fe_type(fe_ctx, fe_eval(fe_ctx, fe_symbol(fe_ctx, "height"))) == FE_TNUMBER) {
		fe_Object *fe_height = fe_eval(fe_ctx, fe_symbol(fe_ctx, "height"));
		config.height = (size_t)fe_tonumber(fe_ctx, fe_height);
	}

	fe_restoregc(fe_ctx, gc);

	SDL_SetWindowTitle(window, config.title);

	memory_sz = DISPLAY_START + (config.width * config.height);
	memory = malloc(memory_sz);
	memset(memory, 0x0, memory_sz);

	for (size_t i = 0; i < ARRAY_LEN(colors); ++i) {
		size_t addr = PALETTE_START + (i * 4);
		for (size_t b = 0; b < 4; ++b) {
			size_t byte = colors[i] >> ((3 - b) * 8);
			memory[addr + b] = byte & 0xFF;
		}
	}

	for (size_t i = 0; i < ARRAY_LEN(font); ++i) {
		for (size_t j = 0; j < FONT_WIDTH; ++j) {
			size_t ch = font[i][j] == 'x' ? 1 : 0;
			memory[FONT_START + (i * FONT_WIDTH) + j] = ch;
		}
	}
}

static void
deinit_fe(void)
{
	fe_close(fe_ctx);
	free(fe_ctx_data);
}

uint32_t _sdl_tick(uint32_t interval, void *param);
uint32_t
_sdl_tick(uint32_t interval, void *param)
{
	SDL_Event ev;
	SDL_UserEvent u_ev;

	ev.type = SDL_USEREVENT;
	u_ev.type = SDL_USEREVENT;
	u_ev.data1 = param;
	ev.user = u_ev;

	SDL_PushEvent(&ev);
	return interval;
}

static bool
init_sdl(void)
{
	if (SDL_Init(SDL_INIT_EVERYTHING))
		return false;

	window = SDL_CreateWindow(
		"cel7 ce",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		config.width * FONT_WIDTH * PIXEL_WIDTH,
		config.height * FONT_HEIGHT * PIXEL_HEIGHT,
		SDL_WINDOW_SHOWN
	);
	if (window == NULL)
		return false;

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return false;

	texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING,
		config.width * FONT_WIDTH, config.height * FONT_HEIGHT
	);
	if (texture == NULL)
		return false;

	SDL_AddTimer(1000 / 30, _sdl_tick, NULL);

	return true;

}

static void
deinit_sdl(void)
{
	if (texture  != NULL) { SDL_DestroyTexture(texture);      }
	if (renderer != NULL) { SDL_DestroyRenderer(renderer);    }
	if (window   != NULL) { SDL_DestroyWindow(window);	  }
	SDL_Quit();
}

static void
load(char *filename)
{
	FILE *fp = fopen(filename, "rb");
	ssize_t gc = fe_savegc(fe_ctx);

	while (true) {
		fe_Object *obj = fe_readfp(fe_ctx, fp);

		// break if there's nothing left to read
		if (!obj) break;

		fe_eval(fe_ctx, obj);

		// restore GC stack which now contains both read object
		// and result
		fe_restoregc(fe_ctx, gc);
	}

	fclose(fp);
}

static void
draw(void)
{
	uint32_t *pixels;
	int       pitch;

	SDL_LockTexture(texture, NULL, (void *)&pixels, &pitch);

	for (size_t dy = 0; dy < config.height; ++dy) {
		for (size_t dx = 0; dx < config.width; ++dx) {
			size_t addr = DISPLAY_START + ((dy * config.width + dx) * 2);
			size_t ch   = (memory[addr + 0]);
			size_t fg_i = (memory[addr + 1] >> 0) & 0xF;
			size_t bg_i = (memory[addr + 1] >> 4) & 0xF;

			if (ch < 32) ch = 32;

			size_t bg_addr = PALETTE_START + (bg_i * 4);
			size_t bg = 0;
			for (size_t b = bg_addr; b < (bg_addr + 4); ++b) bg = (bg << 8) | memory[b];
			bg = (bg << 8) | 0xFF; // Add alpha

			size_t fg_addr = PALETTE_START + (fg_i * 4);
			size_t fg = 0;
			for (size_t b = fg_addr; b < (fg_addr + 4); ++b) fg = (fg << 8) | memory[b];
			fg = (fg << 8) | 0xFF; // Add alpha

			size_t font = FONT_START + ((ch - 32) * FONT_WIDTH * FONT_HEIGHT);

			for (size_t fy = 0; fy < FONT_HEIGHT; ++fy) {
				for (size_t fx = 0; fx < FONT_WIDTH; ++fx) {
					size_t font_ch = memory[font + (fy * FONT_WIDTH + fx)];
					size_t color = font_ch ? fg : bg;
					pixels[(((dy * FONT_HEIGHT) + fy) * (config.width * FONT_WIDTH) + ((dx * FONT_WIDTH) + fx))] = color;
				}
			}
		}
	}

	SDL_UnlockTexture(texture);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
}

static const char *
keyname(size_t kcode)
{
	char *sdl_keys[] = {
		[SDLK_ESCAPE] = "escape",
		[SDLK_RETURN] = "enter",
		[SDLK_r]      = "r",
	};

	switch (kcode) {
	break; case SDLK_UP:    return "up";
	break; case SDLK_DOWN:  return "down";
	break; case SDLK_LEFT:  return "left";
	break; case SDLK_RIGHT: return "right";
	break; default:
		return sdl_keys[kcode] ? sdl_keys[kcode] : "unknown";
	break;
	}
}

static void
run(void)
{
	if (fe_type(fe_ctx, fe_eval(fe_ctx, fe_symbol(fe_ctx, "init"))) == FE_TFUNC) {
		int gc = fe_savegc(fe_ctx);
		fe_Object *objs[1];
		objs[0] = fe_symbol(fe_ctx, "init");
		fe_eval(fe_ctx, fe_list(fe_ctx, objs, 1));
		fe_restoregc(fe_ctx, gc);
	}

	SDL_Event ev;

	while (SDL_WaitEvent(&ev) && !quit) {
		switch (ev.type) {
		break; case SDL_QUIT: {
			quit = true;
		} break; case SDL_KEYDOWN: {
			ssize_t kcode = ev.key.keysym.sym;
			int gc = fe_savegc(fe_ctx);
			fe_Object *objs[2];
			objs[0] = fe_symbol(fe_ctx, "keydown");
			objs[1] = fe_string(fe_ctx, keyname(kcode));
			fe_eval(fe_ctx, fe_list(fe_ctx, objs, 2));
			fe_restoregc(fe_ctx, gc);
		} break; case SDL_KEYUP: {
			ssize_t kcode = ev.key.keysym.sym;

			switch (kcode) {
			break; default: {
				int gc = fe_savegc(fe_ctx);
				fe_Object *objs[2];
				objs[0] = fe_symbol(fe_ctx, "keyup");
				objs[1] = fe_string(fe_ctx, keyname(kcode));
				fe_eval(fe_ctx, fe_list(fe_ctx, objs, 2));
				fe_restoregc(fe_ctx, gc);
			} break;
			}
		} break; case SDL_USEREVENT: {
			int gc = fe_savegc(fe_ctx);
			fe_Object *objs[1];
			objs[0] = fe_symbol(fe_ctx, "step");
			fe_eval(fe_ctx, fe_list(fe_ctx, objs, 1));
			fe_restoregc(fe_ctx, gc);

			draw();

			// Flush user events that may have accumulated if step()
			// took too long
			SDL_FlushEvent(SDL_USEREVENT);
		} break; default: {
		} break;
		}
	}
}

int
main(int argc, char **argv)
{
	init_fe();

	bool sdl_error = !init_sdl();
	if (sdl_error) {
		fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}

	load(argc > 1 ? argv[1] : NULL);
	run();

	deinit_fe();
	deinit_sdl();
}
