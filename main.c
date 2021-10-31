#if defined(__linux__)
#include <unistd.h>
#endif

#include <assert.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "arg.h"
#include "cel7ce.h"
#include "koio.h"
#include "fe.h"
#include "janet.h"

const char *builtin_files[] = {
	"builtin/start.janet", "builtin/setup.janet"
};

static uint32_t colors[] = {
	0x0b0c0d, 0xf7f7e6, 0xf71467, 0xfd971f,
	0xe6d415, 0xa0e01f, 0x46bbff, 0xa98aff,
	0xf9aaaf, 0xab3347, 0x37946e, 0x2a4669,
	0x7c8d99, 0xc2beae, 0x75715e, 0x3e3d32
};

static char *mouse_button_strs[] = {
	[SDL_BUTTON_LEFT]   = "left",
	[SDL_BUTTON_MIDDLE] = "middle",
	[SDL_BUTTON_RIGHT]  = "right",
};

static char *callbacks[MT_COUNT][SC_COUNT] = {
	[MT_Start]  = { [SC_init]  = "I_START_init",  [SC_step] = "I_START_step",
		        [SC_keyup] = "I_START_keyup", [SC_keydown] = "I_START_keydown",
		        [SC_mouse] = "I_START_mouse"
		      },
	[MT_Setup]  = { [SC_init]  = "I_SETUP_init",  [SC_step] = "I_SETUP_step",
		        [SC_keyup] = "I_SETUP_keyup", [SC_keydown] = "I_SETUP_keydown",
		        [SC_mouse] = "I_SETUP_mouse"
		      },
	[MT_Normal] = { [SC_init]  = "init",  [SC_step] = "step",
		        [SC_keyup] = "keyup", [SC_keydown] = "keydown",
		        [SC_mouse] = "mouse"
		       },
};

struct Config config = {
	.title = "cel7 ce",
	.width = 24,
	.height = 16,
	.scale = 4,
	.debug = false,
};

struct Mode mode = {
	.cur = MT_Start,
	.inited = {0},
};

enum LangMode lang = LM_Fe;

uint8_t *memory[BK_COUNT] = {0};
size_t bank = BK_Normal;
size_t color = 1;

JanetTable *janet_env;
void *fe_ctx_data = NULL;
fe_Context *fe_ctx = NULL;
bool quit = false;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

static void
init_vm(void)
{
	// Initialize Janet
	janet_init();
	janet_env = janet_core_env(NULL);
	janet_cfuns(janet_env, "cel7", janet_apis);

	// Initialize fe
	fe_ctx_data = malloc(FE_CTX_DATA_SIZE);
	fe_ctx = fe_open(fe_ctx_data, FE_CTX_DATA_SIZE);

	for (size_t i = 0; i < ARRAY_LEN(fe_apis); ++i) {
		fe_set(
			fe_ctx,
			fe_symbol(fe_ctx, fe_apis[i].name),
			fe_cfunc(fe_ctx, fe_apis[i].func)
		);
	}
}

static void
init_mem(void)
{
	memory[BK_Normal] = calloc(MEMORY_SIZE, sizeof(uint8_t));
	memory[BK_Rom]    = calloc(MEMORY_SIZE, sizeof(uint8_t));

	for (size_t i = 0; i < ARRAY_LEN(colors); ++i) {
		size_t addr = PALETTE_START + (i * 4);
		for (size_t b = 0; b < 4; ++b) {
			size_t byte = colors[i] >> (b * 8);
			memory[BK_Rom][addr + b] = byte & 0xFF;
		}
	}

	for (size_t i = 0; i < ARRAY_LEN(font); ++i) {
		for (size_t j = 0; j < FONT_WIDTH; ++j) {
			size_t ch = font[i][j] == 'x' ? 1 : 0;
			memory[BK_Rom][FONT_START + (i * FONT_WIDTH) + j] = ch;
		}
	}

}

// Set values of config variables in scripting VMs to the values in `config.*`.
// This is run twice. The first time, `config.*` contains the default values,
// so it sets the default values; the second time, `config.*` contains the
// user-provided values, thus re-setting the config values in order to
// propagate the new values across both VMs.
//
static void
set_vals(void)
{
	// Janet
	// TODO: set documentation
	{
		Janet j_title = janet_stringv((const uint8_t *)config.title, strlen(config.title));
		janet_def(janet_env, "title", j_title, "");

		janet_def(janet_env, "width",  janet_wrap_number(config.width), "");
		janet_def(janet_env, "height", janet_wrap_number(config.height), "");
		janet_def(janet_env, "scale",  janet_wrap_number(config.scale), "");
		janet_def(janet_env, "debug",  janet_wrap_boolean(config.debug), "");
	}

	// Fe
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

		objs[0] = fe_symbol(fe_ctx, "=");
		objs[1] = fe_symbol(fe_ctx, "scale");
		objs[2] = fe_number(fe_ctx, config.scale);
		fe_eval(fe_ctx, fe_list(fe_ctx, objs, ARRAY_LEN(objs)));

		objs[0] = fe_symbol(fe_ctx, "=");
		objs[1] = fe_symbol(fe_ctx, "debug");
		objs[2] = fe_bool(fe_ctx, config.debug);
		fe_eval(fe_ctx, fe_list(fe_ctx, objs, ARRAY_LEN(objs)));
	}
}

// Eval inbuilt files
static void
load_builtins(void)
{
	for (size_t i = 0; i < ARRAY_LEN(builtin_files); ++i) {
		FILE *df = ko_fopen(builtin_files[i], "r");
		assert(df != NULL);

		fseek(df, 0L, SEEK_END);
		size_t size = ftell(df);
		fseek(df, 0L, SEEK_SET);

		char *buf = calloc(size, sizeof(char));
		fread(buf, size, sizeof(char), df);
		fclose(df);
		janet_dostring(janet_env, buf, builtin_files[i], NULL);
	}
}

static void
deinit_mem(void)
{
	for (size_t i = 0; i < BK_COUNT; ++i)
		free(memory[i]);
}

static void
deinit_vm(void)
{
	fe_close(fe_ctx);
	free(fe_ctx_data);

	janet_deinit();
}

static uint32_t
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
		config.title,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		config.width * FONT_WIDTH * config.scale,
		config.height * FONT_HEIGHT * config.scale,
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
draw(void)
{
	// TODO: ensure in correct bank

	uint32_t *pixels;
	int       pitch;

	SDL_LockTexture(texture, NULL, (void *)&pixels, &pitch);

	for (size_t dy = 0; dy < config.height; ++dy) {
		for (size_t dx = 0; dx < config.width; ++dx) {
			size_t addr = DISPLAY_START + ((dy * config.width + dx) * 2);
			size_t ch   = (memory[BK_Normal][addr + 0]);
			size_t fg_i = (memory[BK_Normal][addr + 1] >> 0) & 0xF;
			size_t bg_i = (memory[BK_Normal][addr + 1] >> 4) & 0xF;

			// Handle unprintable chars
			if (ch < 32 || ch > 126)
				ch = FONT_FALLBACK_GLYPH;

			size_t bg_addr = PALETTE_START + (bg_i * 4);
			size_t bg = decode_u32_from_bytes(&memory[BK_Normal][bg_addr]);
			bg = (bg << 8) | 0xFF; // Add alpha

			size_t fg_addr = PALETTE_START + (fg_i * 4);
			size_t fg = decode_u32_from_bytes(&memory[BK_Normal][fg_addr]);
			fg = (fg << 8) | 0xFF; // Add alpha

			size_t font = FONT_START + ((ch - 32) * FONT_WIDTH * FONT_HEIGHT);

			for (size_t fy = 0; fy < FONT_HEIGHT; ++fy) {
				for (size_t fx = 0; fx < FONT_WIDTH; ++fx) {
					size_t font_ch = memory[BK_Normal][font + (fy * FONT_WIDTH + fx)];
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
		[SDLK_ESCAPE]  = "escape",
		[SDLK_RETURN]  = "enter",
		[SDLK_a]       = "a", [SDLK_b]      = "b", [SDLK_c]      = "c",
		[SDLK_d]       = "d", [SDLK_e]      = "e", [SDLK_f]      = "f",
		[SDLK_g]       = "g", [SDLK_h]      = "h", [SDLK_i]      = "i",
		[SDLK_j]       = "j", [SDLK_k]      = "k", [SDLK_l]      = "l",
		[SDLK_m]       = "m", [SDLK_n]      = "n", [SDLK_o]      = "o",
		[SDLK_p]       = "p", [SDLK_q]      = "q", [SDLK_r]      = "r",
		[SDLK_s]       = "s", [SDLK_t]      = "t", [SDLK_u]      = "u",
		[SDLK_v]       = "v", [SDLK_w]      = "w", [SDLK_x]      = "x",
		[SDLK_y]       = "y", [SDLK_z]      = "z"
	};

	switch (kcode) {
	break; case SDLK_UP:    return "up";
	break; case SDLK_DOWN:  return "down";
	break; case SDLK_LEFT:  return "left";
	break; case SDLK_RIGHT: return "right";
	break; case SDLK_F1:    return "f1";
	break; case SDLK_F2:    return "f2";
	break; default:
		if (kcode > ARRAY_LEN(sdl_keys) || !sdl_keys[kcode])
			return "unknown";
		return sdl_keys[kcode];
	break;
	}
}

static void
run(void)
{
	SDL_Event ev;

	// Cache current mode for a single cycle. This way, if one callback (say, init())
	// switches the mode, we won't abruptly run another mode's step() without running
	// its init().
	//
	enum ModeType c_mode;

	while (SDL_WaitEvent(&ev) && !quit) {
		c_mode = mode.cur;

		switch (ev.type) {
		break; case SDL_QUIT: {
			quit = true;
		} break; case SDL_KEYDOWN: {
			ssize_t kcode = ev.key.keysym.sym;
			call_func(callbacks[c_mode][SC_keydown], "s", keyname(kcode));
		} break; case SDL_KEYUP: {
			ssize_t kcode = ev.key.keysym.sym;

			switch (kcode) {
			break; case SDLK_ESCAPE:
				quit = true;
			break; default: {
				call_func(callbacks[c_mode][SC_keyup], "s", keyname(kcode));
			} break;
			}
		} break; case SDL_MOUSEMOTION: {
      			double celx = (((double)ev.button.x) /  FONT_WIDTH) / config.scale;
      			double cely = (((double)ev.button.y) / FONT_HEIGHT) / config.scale;
			call_func(callbacks[c_mode][SC_mouse], "snnn", "motion", (double)1, celx, cely);
		} break; case SDL_MOUSEBUTTONDOWN: {
      			double celx = (((double)ev.button.x) /  FONT_WIDTH) / config.scale;
      			double cely = (((double)ev.button.y) / FONT_HEIGHT) / config.scale;
			call_func(callbacks[c_mode][SC_mouse], "snnn", mouse_button_strs[ev.button.button],
				(double)ev.button.clicks, celx, cely);
		} break; case SDL_USEREVENT: {
			if (!mode.inited[c_mode]) {
				call_func(callbacks[c_mode][SC_init], "");
				mode.inited[c_mode] = true;
			}

			call_func(callbacks[c_mode][SC_step], "");
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
	ARGBEGIN {
	break; case 'd':
		config.debug = !config.debug;
	break; case 'v': case 'V':
		printf("cel7ce v"VERSION"\n");
		return 0;
	break; case 'h': default:
		printf("usage: %s [-d] [file]\n", argv[0]);
		printf("       %s [-V]\n", argv[0]);
		printf("       %s [-h]\n", argv[0]);
		return 0;
	} ARGEND

	srand(time(NULL));

	init_mem();
	init_vm();
	set_vals();
	load(*argv);
	load_builtins();
	set_vals();

	bool sdl_error = !init_sdl();
	if (sdl_error) errx(1, "SDL error: %s\n", SDL_GetError());

	run();

	deinit_vm();
	deinit_sdl();
	deinit_mem();

	return 0;
}
