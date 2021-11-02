# Changes

- `Escape` quits immediately right before executing `keydown`.
- `keyup` was removed.
- The new `scale` config value was added.
- `demos/hello.c7` was modified to affect scale and text placement.
- `demos/snake.c7` was modified to comply with stricter requirements (i.e. that
  sprites may not be loaded before init() is called).
- No errors are given if `(>= (* height width) 2048)`.
- Addition of `strlen`, `strstart`, `char->num`, `num->char`, `strat`,
  `username` functions for fe.

# License

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
