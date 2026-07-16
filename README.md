# VT Sokoban (DEC VT DRCS edition)

![VT Sokoban](vtsokoban.jpg)

Sokoban for DEC VT300-series and later terminals, drawn with a downloadable soft
font (DRCS) rather than ASCII. Each map cell is a 16x16 tile made of two soft-font
characters. No curses, no libraries — raw escape sequences and a diffed cell
buffer, so it's playable over a serial line.

## Requirements

- C compiler
- VT320/330/340/420/510/520, or an emulator with DECDLD soft-font support

## Building

```
make
```

## Running

```
./vtsokoban
```

Terminal geometry is auto-detected via DA1/DA2. Flags:

```
  -t 320|340|420  Force font geometry, skip the DA probe
  -w 5..15        Override DRCS glyph width to match emulator cell grid
  -noquery        Skip the DA1/DA2 probe
  -shot FILE      Dump an ASCII approximation of level 1 and exit
  -h              Help
```

If your terminal reports no DRCS support, force a font: `./vtsokoban -t 340`.

## Controls

- Move: arrows, WASD, or HJKL
- `R` restart, `N` next, `P` previous
- `C` redraw, `Q` quit

## Font check

`make demo` writes `soko_demo{420,340,320}.vt`. `cat` the matching one to your
terminal to verify the soft font:

```
make demo
cat soko_demo340.vt
```

## Tiles

Hand-drawn 1-bit 16x16 tiles in `mktiles.c`. Edit the pixel maps and run `make`
to regenerate `soko_tiles.h`.

## Levels

"sokohard" format from https://github.com/mezpusz/sokohard, embedded from
`levels/` by `embed_levels`. See `generate_level.sh`.

## License

Public Domain
