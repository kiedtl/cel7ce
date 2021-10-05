#if defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <assert.h>
#include <err.h>
#include <SDL.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "cel7ce.h"
#include "fe.h"
#include "janet.h"

static uint32_t colors[] = {
	0x0b0c0d, 0xf7f7e6, 0xf71467, 0xfd971f,
	0xe6d415, 0xa0e01f, 0x46bbff, 0xa98aff,
	0xf9aaaf, 0xab3347, 0x37946e, 0x2a4669,
	0x7c8d99, 0xc2beae, 0x75715e, 0x3e3d32
};

struct Config config = {
	.title = "cel7 ce",
	.width = 24,
	.height = 16,
	.scale = 4,
	.debug = false,
};

enum Mode {
	M_Fe, M_Janet
} mode = M_Fe;

uint8_t *memory = NULL;
size_t memory_sz = 0;
size_t color = 1;

JanetTable *janet_env;
void *fe_ctx_data = NULL;
fe_Context *fe_ctx = NULL;
bool quit = false;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

static void
call_func(const char *fnname, const char *arg_fmt, ...)
{
	size_t argc = strlen(arg_fmt);
	va_list ap;
	va_start(ap, arg_fmt);

	if (mode == M_Fe) {
		fe_Object *fnsym = fe_symbol(fe_ctx, fnname);
		if (fe_type(fe_ctx, fe_eval(fe_ctx, fnsym)) == FE_TFUNC) {
			int gc = fe_savegc(fe_ctx);

			fe_Object **objs = calloc(argc + 1, sizeof(fe_Object *));
			objs[0] = fnsym;

			for (size_t i = 0; i < argc; ++i) {
				switch (arg_fmt[i]) {
				break; case 's': {
					objs[i + 1] = fe_string(fe_ctx, va_arg(ap, void *));
				} break; default: {
					assert(false);
				} break;
				}
			}

			fe_eval(fe_ctx, fe_list(fe_ctx, objs, argc + 1));
			free(objs);
			fe_restoregc(fe_ctx, gc);
		}
	} else {
		JanetSymbol j_sym = janet_csymbol(fnname);
		JanetBinding j_binding = janet_resolve_ext(janet_env, j_sym);
		if (j_binding.type != JANET_BINDING_NONE) {
			if (!janet_checktype(j_binding.value, JANET_FUNCTION)) {
				janet_panicf("Binding '%s' must be a function", fnname);
			}

			Janet *args = calloc(argc, sizeof(Janet));
			for (size_t i = 0; i < argc; ++i) {
				switch (arg_fmt[i]) {
				break; case 's': {
					char *str = va_arg(ap, void *);
					args[i] = janet_stringv((const uint8_t *)str, strlen(str));
				} break; default: {
					assert(false);
				} break;
				}
			}

			JanetFunction *j_fn = janet_unwrap_function(j_binding.value);
			Janet out;
			janet_pcall(j_fn, argc, args, &out, NULL);
			free(args);
		}
	}

	va_end(ap);
}

static void
get_string_global(char *name, char *buf, size_t sz)
{
	if (mode == M_Fe) {
		ssize_t gc = fe_savegc(fe_ctx);
		fe_Object *var = fe_eval(fe_ctx, fe_symbol(fe_ctx, name));
		if (fe_type(fe_ctx, var) == FE_TSTRING) {
			fe_tostring(fe_ctx, var, buf, sz);
		} else {
			// TODO: error
		}
		fe_restoregc(fe_ctx, gc);
	} else if (mode == M_Janet) {
		JanetSymbol j_namesym = janet_symbol((uint8_t *)name, strlen(name));
		JanetBinding j_binding = janet_resolve_ext(janet_env, j_namesym);

		if (j_binding.type == JANET_BINDING_NONE) {
			janet_panicf("Global '%s' not set", name);
		} else if (j_binding.type != JANET_BINDING_DEF
				&& j_binding.type != JANET_BINDING_VAR) {
			janet_panicf("Global '%s' must be a string definition", name);
		} else {
			if (!janet_checktype(j_binding.value, JANET_STRING)) {
				janet_panicf("Global '%s' must be a string", name);
			}

			const char *str = (char *)janet_unwrap_string(j_binding.value);
			strncpy(buf, str, sz);
		}
	}
}

static float
get_number_global(char *name)
{
	if (mode == M_Fe) {
		ssize_t gc = fe_savegc(fe_ctx);
		fe_Object *var = fe_eval(fe_ctx, fe_symbol(fe_ctx, name));
		if (fe_type(fe_ctx, var) == FE_TNUMBER) {
			return fe_tonumber(fe_ctx, var);
		} else {
			// TODO: error
			return 0;
		}
		fe_restoregc(fe_ctx, gc);
	} else if (mode == M_Janet) {
		JanetSymbol j_namesym = janet_symbol((uint8_t *)name, strlen(name));
		JanetBinding j_binding = janet_resolve_ext(janet_env, j_namesym);

		if (j_binding.type == JANET_BINDING_NONE) {
			janet_panicf("Global '%s' not set", name);
		} else if (j_binding.type != JANET_BINDING_DEF
				&& j_binding.type != JANET_BINDING_VAR) {
			janet_panicf("Global '%s' must be a number definition", name);
		} else {
			if (!janet_checktype(j_binding.value, JANET_NUMBER)) {
				janet_panicf("Global '%s' must be a number", name);
			}

			return janet_unwrap_number(j_binding.value);
		}
	} else return 0;
}

static void
init_mem(void)
{
	memory_sz = DISPLAY_START + (500 * 500);
	memory = malloc(memory_sz);
	memset(memory, 0x0, memory_sz);

	for (size_t i = 0; i < ARRAY_LEN(colors); ++i) {
		size_t addr = PALETTE_START + (i * 4);
		for (size_t b = 0; b < 4; ++b) {
			size_t byte = colors[i] >> (b * 8);
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
deinit_mem(void)
{
	free(memory);
}

static void
deinit_vm(void)
{
	if (mode == M_Fe) {
		fe_close(fe_ctx);
		free(fe_ctx_data);
	} else if (mode == M_Janet) {
		janet_deinit();
	}
}

static char
_fe_read(fe_Context *ctx, void *udata)
{
	UNUSED(ctx);
	char c = **(char **)udata;
	(*(char **)udata)++;
	return c;
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

	SDL_AddTimer(1000 / 40, _sdl_tick, NULL);

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
load(char *user_filename)
{
	bool fileisbin = false;
	char filename[4096] = {0};

	if (user_filename == NULL) {
		fileisbin = true;

#if defined(__linux__)
		readlink("/proc/self/exe", filename, ARRAY_LEN(filename));
#elif defined(_WIN32) || defined(__WIN32__)
		LPWSTR tbuffer[4096];
		DWORD sz = GetModuleFileName(NULL, tbuffer, ARRAY_LEN(tbuffer));
		if (sz == 0) { // error
			switch (GetLastError()) {
			break; case ERROR_INSUFFICIENT_BUFFER:
				errx(1, "Couldn't get path to executable: path too large");
			break; case ERROR_SUCCESS: default:
				errx(1, "Couldn't get path to executable: unknown error");
			break;
			}
		}
		wcstombs(filename, tbuffer, sz);
#else
		errx(1, "No cartridge or file provided.");
#endif
	} else {
		strncpy(filename, user_filename, ARRAY_LEN(filename));
		filename[ARRAY_LEN(filename) - 1] = '\0';
	}


	if (!fileisbin) {
		char *dot = strrchr(filename, '.');
		if (dot && !strcmp(dot, ".fe")) {
			mode = M_Fe;
		} else if (dot && !strcmp(dot, ".janet")) {
			mode = M_Janet;
		}
	}

	struct stat st;
	ssize_t stat_res = stat(filename, &st);
	if (stat_res == -1) err(1, "Cannot stat file '%s'", filename);

	char *filebuf = calloc(st.st_size, sizeof(char));
	FILE *fp = fopen(filename, "rb");
	fread(filebuf, st.st_size, sizeof(char), fp);
	fclose(fp);

	char *start = filebuf;
	if (fileisbin) {
		size_t last0 = 0;
		for (size_t i = 0; i < st.st_size; ++i)
			if (filebuf[i] == '\0') last0 = i;
		start = &filebuf[last0 + 1];
	}

	if (mode == M_Fe) {
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

			objs[0] = fe_symbol(fe_ctx, "=");
			objs[1] = fe_symbol(fe_ctx, "scale");
			objs[2] = fe_number(fe_ctx, config.scale);
			fe_eval(fe_ctx, fe_list(fe_ctx, objs, ARRAY_LEN(objs)));
		}


		ssize_t gc = fe_savegc(fe_ctx);
		while (true) {
			fe_Object *obj = fe_read(fe_ctx, _fe_read, (void *)&start);

			// break if there's nothing left to read
			if (!obj) break;

			fe_eval(fe_ctx, obj);

			// restore GC stack which now contains both read object
			// and result
			fe_restoregc(fe_ctx, gc);
		}
	} else {
		janet_init();
		janet_env = janet_core_env(NULL);
		janet_cfuns(janet_env, "cel7", janet_apis);

		// Set default values of variables
		// TODO: set documentation
		{
			Janet j_title = janet_stringv((const uint8_t *)config.title, strlen(config.title));
			janet_def(janet_env, "title", j_title, "");

			Janet j_width = janet_wrap_number(config.width);
			janet_def(janet_env, "width", j_width, "");

			Janet j_height = janet_wrap_number(config.height);
			janet_def(janet_env, "height", j_height, "");

			Janet j_scale = janet_wrap_number(config.scale);
			janet_def(janet_env, "scale", j_scale, "");
		}

		janet_dostring(janet_env, start, filename, NULL);
	}

	get_string_global("title", config.title, ARRAY_LEN(config.title));
	config.width = get_number_global("width");
	config.height = get_number_global("height");
	config.scale = get_number_global("scale");
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

			// Handle unprintable chars
			if (ch < 32 || ch > 126)
				ch = FONT_FALLBACK_GLYPH;

			size_t bg_addr = PALETTE_START + (bg_i * 4);
			size_t bg = decode_u32_from_bytes(&memory[bg_addr]);
			bg = (bg << 8) | 0xFF; // Add alpha

			size_t fg_addr = PALETTE_START + (fg_i * 4);
			size_t fg = decode_u32_from_bytes(&memory[fg_addr]);
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
	call_func("init", "");

	SDL_Event ev;

	while (SDL_WaitEvent(&ev) && !quit) {
		switch (ev.type) {
		break; case SDL_QUIT: {
			quit = true;
		} break; case SDL_KEYDOWN: {
			ssize_t kcode = ev.key.keysym.sym;
			call_func("keydown", "s", keyname(kcode));
		} break; case SDL_KEYUP: {
			ssize_t kcode = ev.key.keysym.sym;

			switch (kcode) {
			break; case SDLK_ESCAPE:
				quit = true;
			break; default: {
				call_func("keyup", "s", keyname(kcode));
			} break;
			}
		} break; case SDL_USEREVENT: {
			call_func("step", "");
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
	srand(time(NULL));

	init_mem();
	load(argc > 1 ? argv[1] : NULL);

	bool sdl_error = !init_sdl();
	if (sdl_error) {
		fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}

	run();

	deinit_vm();
	deinit_sdl();
	deinit_mem();
}
