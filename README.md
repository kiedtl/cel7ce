# cel7: community edition

## What

cel7ce is an open-source reimplementation of
[cel7](https://github.com/kiedtl/cel7). It aims to be a bit more polished and
feature-rich while being (almost) completely backwards-compatible with the
original cel7.

### New features

- Support for [janet](https://janet-lang.org)
- *Two* memory banks.
- Fancy loading animation.
- Others I've forgotten.
- Addition of `strlen`, `strstart`, `char->num`, `num->char`, `strat`,
  `username` functions for fe.
- A new `scale` script config value.

### Breaking changes

- `Escape` quits immediately, not executing `keydown`.
- The `keyup` callback was removed.
- It is no longer possible to load sprites into memory before init() is
  called.
  - `demos/snake.c7` was modified to comply with these requirements.

### Non-breaking changes

- `demos/hello.c7` was modified to affect scale and text placement.
- No errors are given if `(>= (* height width) 2048)`.

## Why

The original cel7 seems abandoned at present; no updates have been posted in
a while and many parts are thoroughly undocumented. Additionally, as no
source code is available, there's no way to run it on platforms that a binary
isn't provided for, like a Raspberry Pi. Due to these and other issues, I
decided to do a reimplementation in order to continue the project.

## License

All code, with the exception of `demos/bonsai.c7`, `demos/hello.c7`,
`demos/glitch.c7`, `demos/maze.c7`, and `demos/snake.c7` are licensed under the
GPLv3 license.

The previously-mentioned demos, except `demos/bonsai.c7`, are copyright
[@rxi](https://github.com/rxi) and are not, as far as I'm aware, under any open
source license. You can find the original versions
[here](https://rxi.itch.io/cel7).

`demos/bonsai.c7` was ported from [jallbrit's bonsai.sh
script](https://gitlab.com/jallbrit/bonsai.sh), and is also licensed under the
GPLv3 license.
