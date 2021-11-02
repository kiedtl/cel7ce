M_USERNAME = $(USER)
ifeq ($(OS),Windows_NT)
	M_USERNAME = $(USERNAME)
endif

NAME     = cel7
VERSION  = 0.1.0

include config.mk

BIN      = $(NAME)
SRC      = assets.c janet_api.c fe_api.c font.c util.c \
	   third_party/fe/src/fe.c third_party/janet/janet.c main.c
ASSETS   = builtin/start.janet builtin/setup.janet builtin/error.janet
OBJ      = $(SRC:.c=.o)

KOIO_BIN = third_party/koio/build/koio
KOIO_AR  = third_party/koio/build/koio.a
KOIO_SRC = third_party/koio/lib/ko_add_alias.c third_party/koio/lib/ko_add_file.c \
	   third_party/koio/lib/ko_del_file.c third_party/koio/lib/ko_fopen.c \
	   third_party/koio/lib/hashtable.c
KOIO_OBJ = $(KOIO_SRC:.c=.o)

# Use the correct file extensions on Windows.
ifeq ($(OS),Windows_NT)
	BIN = $(NAME).exe
	OBJ = $(SRC:.c=.obj)
	KOIO_OBJ = $(KOIO_SRC:.c=.obj)
endif

CC       = $(UNIX_CC)
LD       = $(UNIX_LD)
ifeq ($(OS),Windows_NT)
	CC = $(WIN_CC)
	LD = $(WIN_LD)
endif

WARNING  = -Wall -Wpedantic -Wextra -Wold-style-definition -Wmissing-prototypes \
	   -Winit-self -Wfloat-equal -Wstrict-prototypes -Wredundant-decls \
	   -Wendif-labels -Wstrict-aliasing=2 -Woverflow -Wformat=2 -Wtrigraphs \
	   -Wmissing-include-dirs -Wno-format-nonliteral -Wunused-parameter \
	   -Wincompatible-pointer-types \
	   -Werror=implicit-function-declaration -Werror=return-type

DEF      = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=1000 -D_DEFAULT_SOURCE \
	   -D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L
INCL     = -Ithird_party/fe/src/ -Ithird_party/janet/ -Ithird_party/koio/include/ -I.

# Defines:
# -DSDL_DISABLE_ANALYZE_MACROS removes the need for the nonstandard sal.h on
# Windows, which may not exist depending on how the vcredist was installed.
#
# -D_CRT_SECURE_NO_WARNINGS remove some idiotic warnings on Windows (something
# along the lines of "fopen isn't safe! use our fopen_s instead !!")
ifeq ($(OS),Windows_NT)
	DEF     += -DSDL_DISABLE_ANALYZE_MACROS -D_CRT_SECURE_NO_WARNINGS
	INCL    += -I$(WIN_SDL_INC)
	WARNING += -Wno-newline-eof -Wno-language-extension-token
endif

CFLAGS   = -Og -g $(DEF) $(INCL) $(WARNING) -funsigned-char
LDFLAGS  = -fuse-ld=$(LD) -lSDL2

ifeq ($(OS),Windows_NT)
	LDFLAGS  += -L$(WIN_SDL_LIB) $(WIN_LDFLAGS)
else
	CFLAGS   += $(shell sdl2-config --cflags)
	LDFLAGS  += $(shell sdl2-config --libs)
endif

# -----------------------------------------------------------------------------

.PHONY: all
all: $(BIN)

define make_object
	@printf "    %-8s%s\n" "CC" $@
	$(CMD)$(CC) -o $@ -c $< $(CFLAGS)
endef

ifeq ($(OS),Windows_NT)
%.obj: %.c
	$(make_object)
else
%.o: %.c
	$(make_object)
endif

assets.c: $(ASSETS) $(KOIO_BIN)
	@printf "    %-8s%s\n" "KOIO" $@
	$(CMD)third_party/koio/build/koio -o assets.c $(ASSETS)

$(KOIO_BIN): $(KOIO_AR)
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ third_party/koio/tool/main.c $(KOIO_AR) \
		-Ithird_party/koio/include
	
$(KOIO_AR): $(KOIO_OBJ)
	@printf "    %-8s%s\n" "AR" $@
	$(CMD)ar rvs $@ $^ >/dev/null

$(BIN): main.c $(OBJ) $(KOIO_AR) builtin/default.fe
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ $(OBJ) $(KOIO_AR) $(CFLAGS) $(LDFLAGS)
	$(CMD)printf '\0' >> $(BIN)
	$(CMD)cat builtin/default.fe >> $(BIN)

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJ) $(KOIO_BIN) $(KOIO_AR) $(KOIO_OBJ)
	rm -f assets.c *.lib *.pdb *.o *.obj
