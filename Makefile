# Portable Makefile: fully literal recipes -- no $(...) variables, no $@/$<
# automatic variables, no -Wall.  This avoids every known old make/shell
# incompatibility.  If your C compiler is not "cc", edit the lines below
# (e.g. change cc to gcc).

all: vtsokoban

# Host tool that packages the tile art into soko_tiles.h
mktiles: mktiles.c
	cc -o mktiles mktiles.c

soko_tiles.h: mktiles
	./mktiles -o soko_tiles.h

# Host tool that embeds the level files
embed_levels: embed_levels.c
	cc -o embed_levels embed_levels.c

embedded_levels.h: embed_levels
	./embed_levels

# The game: DEC VT DRCS front end, no curses
vtsokoban: vtsokoban.c vt_term.c vt.h soko_tiles.h embedded_levels.h levels.h
	cc -o vtsokoban vtsokoban.c vt_term.c

# Regenerate the hardware tile-demo files (soko_demo{420,340,320}.vt)
demo: mktiles
	./mktiles -d soko_demo

run: vtsokoban
	./vtsokoban

clean:
	rm -f vtsokoban mktiles embed_levels soko_tiles.h embedded_levels.h
	rm -f soko_demo320.vt soko_demo340.vt soko_demo420.vt
	rm -rf *.dSYM

.PHONY: all run clean demo
