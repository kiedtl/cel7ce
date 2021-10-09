M_USERNAME = $(USER)
ifeq ($(OS),Windows_NT)
	M_USERNAME = $(USERNAME)
endif

NAME     = cel7
VERSION  = 0.1.0

include config.mk

BIN      = $(NAME)
SRC      = janet_api.c fe_api.c font.c util.c third_party/fe/src/fe.c \
	   third_party/janet/janet.c main.c
OBJ      = $(SRC:.c=.o)

# Use the correct file extensions on Windows.
ifeq ($(OS),Windows_NT)
	BIN = $(NAME).exe
	OBJ = $(SRC:.c=.obj)
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
	   -Werror=implicit-function-declaration -Werror=return-type \
	   -Wno-newline-eof -Wno-language-extension-token

DEF      = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=1000 -D_DEFAULT_SOURCE -D_GNU_SOURCE
INCL     = -Ithird_party/fe/src/ -Ithird_party/janet/ -I.

# Defines:
# -DSDL_DISABLE_ANALYZE_MACROS removes the need for the nonstandard sal.h on
# Windows, which may not exist depending on how the vcredist was installed.
#
# -D_CRT_SECURE_NO_WARNINGS remove some idiotic warnings on Windows (something
# along the lines of "fopen isn't safe! use our fopen_s instead !!")
ifeq ($(OS),Windows_NT)
	DEF  += -DSDL_DISABLE_ANALYZE_MACROS -D_CRT_SECURE_NO_WARNINGS
	INCL += -I$(WIN_SDL_INC)
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

$(BIN): main.c $(OBJ) def.c7
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)
	$(CMD)printf '\0' >> $(BIN)
	$(CMD)cat def.c7 >> $(BIN)

.PHONY: clean
clean:
	rm -f $(BIN)
	rm -f *.lib *.pdb
	rm -f *.o *.obj