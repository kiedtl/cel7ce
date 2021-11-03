#ifndef CEL7CE_H
#define CEL7CE_H

#if defined(_WIN32) || defined(__WIN32__)
#define err(e, ...) do { \
	fprintf(stderr, __VA_ARGS__); \
	perror(": "); \
	exit(e); \
} while (0);
#define errx(e, ...) do { \
	fprintf(stderr, __VA_ARGS__); \
	exit(e); \
} while (0);
#elif defined(__linux__)
#include <err.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include <sys/time.h>

#include "fe.h"
#include "janet.h"

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#define MEMORY_SIZE         0x7fff
#define PALETTE_START       0x4000
#define FONT_START          0x4040
#define DISPLAY_START       0x52a0    /* bank 1 */
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

struct Mode {
	enum ModeType {
		MT_Start  = 0,
		MT_Setup  = 1,
		MT_Normal = 2,
		MT_Error  = 3,
		MT_COUNT,
	} cur;
	_Bool inited[MT_COUNT];
	size_t steps[MT_COUNT];
};

struct ApiFunc {
	const char *name;
	fe_Object *(*func)(fe_Context *, fe_Object *);
};

enum ScriptCallback {
	SC_init    = 0,
	SC_step    = 1,
	SC_keydown = 2,
	SC_mouse   = 3,
	SC_COUNT,
};

enum Bank {
	BK_Normal = 0,
	BK_Rom    = 1,
	BK_COUNT,
};

enum LangMode {
	LM_Fe, LM_Janet
};

extern struct Config config;
extern struct Mode mode;

// Should we switch to MT_Error mode after setup?
// Set to true if there was an error in eval.
extern _Bool load_error;

extern enum LangMode lang;

extern struct timeval delay_set;
extern struct timeval delay_val;

extern uint8_t *memory[BK_COUNT];
extern size_t bank;
extern size_t color;

extern JanetTable *janet_env;
extern void *fe_ctx_data;
extern fe_Context *fe_ctx;
extern bool quit;

extern jmp_buf fe_error_recover;

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture *texture;

extern const char font[96 * FONT_HEIGHT][FONT_WIDTH];
extern const struct JanetReg janet_apis[16];
extern const struct ApiFunc fe_apis[18];

uint32_t decode_u32_from_bytes(uint8_t *bytes);
char *get_username(void);
void load(char *user_filename);
void call_func(const char *fnname, const char *arg_fmt, ...);
void get_string_global(char *name, char *buf, size_t sz);
float get_number_global(char *name);

#endif
