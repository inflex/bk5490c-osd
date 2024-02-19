# 
# VERSION CHANGES
#

BV=$(shell (git rev-list HEAD --count))
BV=1000
BD=$(shell (date))

CROSS=i686-w64-mingw32.static-
WINSDLCFG=/home/pld/development/others/mxe/usr/i686-w64-mingw32.static/bin/sdl2-config
LOCATION=/usr/local
CFLAGS=-O2 -DBUILD_VER="$(BV)"  -DBUILD_DATE=\""$(BD)"\"
SDL_FLAGS=$(shell /home/pld/development/others/mxe/usr/i686-w64-mingw32.static/bin/sdl2-config --cflags )
SDL_LIBS=$(shell /home/pld/development/others/mxe/usr/i686-w64-mingw32.static/bin/sdl2-config --libs )
GCC=$(CROSS)gcc
GPP=$(CROSS)g++
LD=$(CROSS)ld
AR=$(CROSS)ar

EXTRA_LIBS=-lSDL2_ttf -lfreetype -lbz2 -lz -lharfbuzz -lpng -lzip -l:libz.a
WINLIBS=-lgdi32 -lcomdlg32 -lcomctl32 -lmingw32 -lharfbuzz -lfreetype $(SDL_LIBS) $(EXTRA_LIBS)
WINFLAGS= -municode -static-libgcc -static-libstdc++  -Wunused-variable $(SDL_FLAGS)

WINOBJ=bk5490c.exe

.c.o:
	$(GCC) $(CFLAGS) $(COMPONENTS) -c $*.c

.cpp.o:
	$(GPP) $(CFLAGS) $(COMPONENTS) -c $*.cpp

OFILES=flog.o
win: $(OFILES)
	@echo Build Release $(BV)
	@echo Build Date $(BD)
	@echo SDL2 flags $(SDL_FLAGS)
	@echo SDL2 libs $(SDL_LIBS)
	@echo
	@echo
	$(GPP) $(CFLAGS) $(WINFLAGS) bk5490c.cpp $(OFILES) -o $(WINOBJ) $(WINLIBS)

strip: 
	strip *.exe

clean:
	rm -f *.o *core $(WINOBJ)

default: $(WINOBJ)
