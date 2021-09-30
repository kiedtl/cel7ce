CMD      = @

VERSION  = 0.1.0
NAME     = cel7
SRC      = api.c font.c util.c third_party/fe/src/fe.c
OBJ      = $(SRC:.c=.o)

WARNING  = -Wall -Wpedantic -Wextra -Wold-style-definition -Wmissing-prototypes \
	   -Winit-self -Wfloat-equal -Wstrict-prototypes -Wredundant-decls \
	   -Wendif-labels -Wstrict-aliasing=2 -Woverflow -Wformat=2 -Wtrigraphs \
	   -Wmissing-include-dirs -Wno-format-nonliteral -Wunused-parameter \
	   -Wincompatible-pointer-types \
	   -Werror=implicit-function-declaration -Werror=return-type

DEF      = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=1000 -D_DEFAULT_SOURCE
INCL     = -Ithird_party/fe/src/
CC       = cc
CFLAGS   = -Og -g $(DEF) $(INCL) $(WARNING) -funsigned-char $(shell sdl2-config --cflags)
LD       = bfd
LDFLAGS  = -fuse-ld=$(LD) -L/usr/include -lm -lexecinfo $(shell sdl2-config --libs)

.PHONY: all
all: $(NAME)

%.o: %.c
	@printf "    %-8s%s\n" "CC" $@
	$(CMD)$(CC) -o $@ -c $< $(CFLAGS)

$(NAME): main.c $(OBJ) def.c7
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ main.c $(OBJ) $(CFLAGS) $(LDFLAGS)
	$(CMD)printf '\0' >> $(NAME)
	$(CMD)cat def.c7 >> $(NAME)

.PHONY: clean
clean:
	rm -rf $(NAME) $(OBJ)
