CMD = @

UNIX_CC      = cc
UNIX_LD      = bfd

WIN_CC       = clang
WIN_LD       = lld-link

WIN_SDL_INC  = C:\Users\$(M_USERNAME)\scoop\apps\sdl2\current\include\SDL2
WIN_SDL_LIB  = C:\Users\$(M_USERNAME)\scoop\apps\sdl2\current\lib\ 

WIN_LDFLAGS  = -lSDL2main -Xlinker /subsystem:console