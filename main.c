#if defined(__linux__)
#include <unistd.h>
#endif

#include <assert.h>
#include <gif_lib.h>
#include <SDL.h>
#include <setjmp.h>
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
#include "vec.h"

const char *builtin_files[] = {
	"builtin/start.janet", "builtin/setup.janet", "builtin/error.janet"
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
		        [SC_keydown] = "I_START_keydown", [SC_mouse] = "I_START_mouse"
		      },
	[MT_Setup]  = { [SC_init]  = "I_SETUP_init",  [SC_step] = "I_SETUP_step",
		        [SC_keydown] = "I_SETUP_keydown", [SC_mouse] = "I_SETUP_mouse"
		      },
	[MT_Normal] = { [SC_init]  = "init",  [SC_step] = "step",
		        [SC_keydown] = "keydown", [SC_mouse] = "mouse"
		      },
	[MT_Error]  = { [SC_init]  = "I_ERROR_init",  [SC_step] = "I_ERROR_step",
		        [SC_keydown] = "I_ERROR_keydown", [SC_mouse] = "I_ERROR_mouse"
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
_Bool load_error = false;

enum LangMode lang = LM_Fe;

// XXX: delays set in one mode continue through mode switches in purpose.
// (This is so that the startup animation code can sleep a bit before
// continuing with the cartridge execution.)
struct timeval delay_set = {0};
struct timeval delay_val = {0};

uint8_t *memory[BK_COUNT] = {0};
size_t bank = BK_Normal;
uint8_t color = 1;

JanetTable *janet_env;
void *fe_ctx_data = NULL;
fe_Context *fe_ctx = NULL;
bool quit = false;

jmp_buf fe_error_recover;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

static _Bool has_recording = false;
static _Bool is_recording = false;
static vec_void_t frames;

static void
_fe_error(fe_Context *ctx, const char *err, fe_Object *cl)
{
	fprintf(stderr, "fe error: %s\n", err);
	for (; !fe_isnil(ctx, cl); cl = fe_cdr(ctx, cl)) {
		char buf[128];
		fe_tostring(ctx, fe_car(ctx, cl), buf, ARRAY_LEN(buf));
		fprintf(stderr, "=> %s\n", buf);
	}

	longjmp(fe_error_recover, 1);
}

static void
init_vm(void)
{
	// Initialize Janet
	// {{{
	janet_init();
	janet_env = janet_core_env(NULL);
	janet_cfuns(janet_env, "cel7", janet_apis);
	// }}}

	// Initialize fe
	// {{{
	fe_ctx_data = malloc(FE_CTX_DATA_SIZE);
	fe_ctx = fe_open(fe_ctx_data, FE_CTX_DATA_SIZE);

	for (size_t i = 0; i < ARRAY_LEN(fe_apis); ++i) {
		fe_set(
			fe_ctx,
			fe_symbol(fe_ctx, fe_apis[i].name),
			fe_cfunc(fe_ctx, fe_apis[i].func)
		);
	}

	fe_Handlers *hnds = fe_handlers(fe_ctx);
	assert(hnds != NULL);
	hnds->error = _fe_error;
	// }}}
}

static void
init_mem(void)
{
	memory[BK_Normal] = ecalloc(MEMORY_SIZE, sizeof(uint8_t));
	memory[BK_Rom]    = ecalloc(MEMORY_SIZE, sizeof(uint8_t));

	// Initialize colors.
	for (size_t i = 0; i < ARRAY_LEN(colors); ++i) {
		size_t addr = PALETTE_START + (i * 4);
		for (size_t b = 0; b < 4; ++b) {
			size_t byte = colors[i] >> (b * 8);
			memory[BK_Rom][addr + b] = byte & 0xFF;
		}
	}

	// Initialize fonts.
	for (size_t i = 0; i < ARRAY_LEN(font); ++i) {
		for (size_t j = 0; j < FONT_WIDTH; ++j) {
			size_t ch = font[i][j] == 'x' ? 1 : 0;
			memory[BK_Rom][FONT_START + (i * FONT_WIDTH) + j] = ch;
		}
	}

	// Initialize display portion of BK_Rom to some stuff. This will only
	// be seen if the user accidentally switches banks across step() calls
	// and cel7 ends up drawing from BK_Rom.
	//
	for (size_t i = DISPLAY_START; i < MEMORY_SIZE; ++i) {
		//memory[BK_Rom][i] = "\xAA\xBB\xCC\xDD\xEE\xFF"[i % 5];
		memory[BK_Rom][i] = "BLACKLIVESMATTER"[i % 16];
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
	// Documentation deliberately elided.
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

		char *buf = ecalloc(size, sizeof(char));
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
	assert(fe_ctx != NULL);

	fe_close(fe_ctx);
	free(fe_ctx_data);

	fe_ctx = NULL;
	fe_ctx_data = NULL;

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
	SDL_StartTextInput();

	vec_init(&frames);

	return true;
}

static void
deinit_sdl(void)
{
	if (texture  != NULL) { SDL_DestroyTexture(texture);      }
	if (renderer != NULL) { SDL_DestroyRenderer(renderer);    }
	if (window   != NULL) { SDL_DestroyWindow(window);	  }
	SDL_Quit();

	int i;
	uint32_t *frame;
	vec_foreach(&frames, frame, i) {
		free(frame);
	}

	vec_deinit(&frames);
}

static void
draw(void)
{
	uint32_t *pixels;
	int       pitch;

	uint32_t *frame = NULL;
	if (is_recording) {
		frame = ecalloc(
			config.height * FONT_HEIGHT * config.width * FONT_WIDTH,
			sizeof(uint32_t)
		);
	}

	SDL_LockTexture(texture, NULL, (void *)&pixels, &pitch);

	for (size_t dy = 0; dy < config.height; ++dy) {
		for (size_t dx = 0; dx < config.width; ++dx) {
			size_t addr = DISPLAY_START + ((dy * config.width + dx) * 2);
			size_t ch   = (memory[bank][addr + 0]);
			size_t fg_i = (memory[bank][addr + 1] >> 0) & 0xF;
			size_t bg_i = (memory[bank][addr + 1] >> 4) & 0xF;

			// Handle unprintable chars
			if (ch < 32 || ch > 126)
				ch = FONT_FALLBACK_GLYPH;

			size_t bg_addr = PALETTE_START + (bg_i * 4);
			size_t bg = decode_u32_from_bytes(&memory[bank][bg_addr]);
			bg = (bg << 8) | 0xFF; // Add alpha

			size_t fg_addr = PALETTE_START + (fg_i * 4);
			size_t fg = decode_u32_from_bytes(&memory[bank][fg_addr]);
			fg = (fg << 8) | 0xFF; // Add alpha

			size_t font = FONT_START + ((ch - 32) * FONT_WIDTH * FONT_HEIGHT);

			for (size_t fy = 0; fy < FONT_HEIGHT; ++fy) {
				for (size_t fx = 0; fx < FONT_WIDTH; ++fx) {
					size_t font_ch = memory[bank][font + (fy * FONT_WIDTH + fx)];
					size_t color = font_ch ? fg : bg;
					size_t addr = (((dy * FONT_HEIGHT) + fy) * (config.width * FONT_WIDTH) + ((dx * FONT_WIDTH) + fx));
					pixels[addr] = color;

					if (is_recording) {
						assert(frame != NULL);
						frame[addr] = color;
					}
				}
			}
		}
	}

	SDL_UnlockTexture(texture);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	if (is_recording) {
		vec_push(&frames, (void *)frame);
	}
}

static void
run(void)
{
	SDL_Event ev;

	// Jump location for fe's error handler.
	ssize_t r = setjmp(fe_error_recover);
	if (r == 1) {
		// fe_eval had a tantrum and we longjmp'd from the error
		// handler we set in init_vm(). Switch to error mode and continue.
		mode.cur = MT_Error;
	}

	// Cache current mode for a single cycle. This way, if one callback
	// (say, init()) switches the mode, we won't abruptly run another
	// mode's step() without running its init().
	//
	enum ModeType c_mode;

	while (SDL_WaitEvent(&ev) && !quit) {
		c_mode = mode.cur;

		switch (ev.type) {
		break; case SDL_QUIT: {
			quit = true;
		} break; case SDL_TEXTINPUT: {
			call_func("keydown", "s", ev.text.text);
		} break; case SDL_KEYDOWN: {
			char *name = NULL;

			ssize_t kcode = ev.key.keysym.sym;
			switch (kcode) {
			break; case SDLK_F1: {
				has_recording = true;
				is_recording = !is_recording;
				printf("recording: %s\n", is_recording ? "yes" : "no");
			}
			break; case SDLK_ESCAPE: quit = true;
			break; case SDLK_RETURN: name = "enter";
			break; case SDLK_UP:     name = "up";
			break; case SDLK_DOWN:   name = "down";
			break; case SDLK_LEFT:   name = "left";
			break; case SDLK_RIGHT:  name = "right";
			break; default:
			break;
			}

			if (name) {
				call_func(callbacks[c_mode][SC_keydown], "s", name);
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
			_Bool has_delay = timerisset(&delay_val);

			struct timeval cur_time;
			struct timeval diff;

			if (has_delay) {
				gettimeofday(&cur_time, NULL);
				timeradd(&delay_set, &delay_val, &diff);
			}

			if (!has_delay || timercmp(&cur_time, &diff, >)) {
				if (has_delay) timerclear(&delay_val);

				++mode.steps[c_mode];

				if (!mode.inited[c_mode]) {
					call_func(callbacks[c_mode][SC_init], "");
					mode.inited[c_mode] = true;
				}

				call_func(callbacks[c_mode][SC_step], "");
				draw();
			}

			// Flush user events that may have accumulated if step()
			// took too long
			SDL_FlushEvent(SDL_USEREVENT);
		} break; default: {
		} break;
		}
	}
}

static void
dump_recording(void)
{
	assert(has_recording);

	int error = 0;

	time_t t = time(NULL);
	char fname[128];
	strftime(fname, sizeof(fname), "recording-%Y%m%d-%H%M%S.gif", localtime(&t));

	size_t g_width = config.width * FONT_WIDTH;
	size_t g_height = config.height * FONT_HEIGHT;

	GifFileType* g_file = EGifOpenFileName(fname, false, &error);
	if (!g_file) goto giflib_error;

	// Output image headers.
	if (EGifPutScreenDesc(g_file, g_width, g_height, 8, 0, NULL) == GIF_ERROR)
		goto giflib_error;

	// Create an image loop with the netscape extension.
	// See: http://www.vurdalakov.net/misc/gif/netscape-looping-application-extension
	// {{{

	// nsle:
	//    "NETSCAPE" == the application identifier
	//    "2.0"      == application authentication code.
	char nsle[12] = "NETSCAPE2.0";

	char subblock[] = {
		1,  // Sub-block id. 1 == identifies the Netscape Looping Extension.
		0,  // Loop count (high byte).
		0   // Loop count (low byte).
	};

	EGifPutExtensionLeader(g_file, APPLICATION_EXT_FUNC_CODE);
	EGifPutExtensionBlock(g_file, ARRAY_LEN(nsle) - 1, nsle);
	EGifPutExtensionBlock(g_file, ARRAY_LEN(subblock), subblock);
	EGifPutExtensionTrailer(g_file);
	// }}}

	int i;
	uint32_t *frame;
	vec_foreach(&frames, frame, i) {
		size_t sz = g_width * g_height;

		GifColorType colors[16] = {0};
		size_t colors_num = 0;

		GifByteType *g_frame_pixels = ecalloc(sz, sizeof(GifByteType));

		// Extract palette and create palette-ified frame for gif.
		for (size_t i = 0; i < sz; ++i) {
			size_t color_index = (size_t)-1;

			uint32_t cur_color = frame[i];
			GifColorType g_cur_color = {0};
			g_cur_color.Red   = (cur_color >> 24) & 0xFF;
			g_cur_color.Green = (cur_color >> 16) & 0xFF;
			g_cur_color.Blue  = (cur_color >>  8) & 0xFF;

			// Does the palette already contain this?
			_Bool contained = false;
			for (size_t c = 0; c < colors_num; ++c) {
				if (
					g_cur_color.Red == colors[c].Red &&
					g_cur_color.Green == colors[c].Green &&
					g_cur_color.Blue == colors[c].Blue
				) {
					contained = true;
					color_index = c;
					break;
				}
			}

			if (!contained) {
				// A frame can only contain up to 16 colors at a time.
				assert(colors_num < ARRAY_LEN(colors));

				colors[colors_num] = g_cur_color;
				color_index = colors_num;
				++colors_num;
			}

			g_frame_pixels[i] = color_index;
		}

		// Set delay via graphics control extension.
		// See spec: http://www.w3.org/Graphics/GIF/spec-gif89a.txt
		//
		// {{{
		uint8_t gce_str[] = {
			0x04, // length of gce_str
			0x00, // misc packed fields (unused)
			0x00, // delay time, 1/100 seconds (u16, continued below)
			0x03, // ...
		};

		if (EGifPutExtension(
			g_file,
			GRAPHICS_EXT_FUNC_CODE,
			ARRAY_LEN(gce_str),
			gce_str
		) == GIF_ERROR)
			goto giflib_error;
		// }}}

		// Put frame headers.
		if (EGifPutImageDesc(
			g_file,
			0, 0, g_width, g_height,
			false,
			GifMakeMapObject(ARRAY_LEN(colors), colors)
		) == GIF_ERROR) goto giflib_error;

		// Put frame, row-wise.
		for (size_t rowptr = 0; rowptr < sz; rowptr += g_width) {
			if (EGifPutLine(
				g_file, &g_frame_pixels[rowptr], g_width
			) == GIF_ERROR) goto giflib_error;
		}
	}

	EGifCloseFile(g_file, &error);
	fprintf(stderr, "Saved %s\n", fname);

	return;

giflib_error:
	warnx("couldn't save recording to '%s': error %d: %s",
		fname, error, GifErrorString(error));
}

int
main(int argc, char **argv)
{
	ARGBEGIN {
	break; case 'd':
		config.debug = !config.debug;
	break; case 'r':
		is_recording = true;
		has_recording = true;
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
	set_vals();
	load_builtins();

	bool sdl_error = !init_sdl();
	if (sdl_error) errx(1, "SDL error: %s\n", SDL_GetError());

	run();

	if (has_recording) dump_recording();

	deinit_vm();
	deinit_sdl();
	deinit_mem();

	return 0;
}
