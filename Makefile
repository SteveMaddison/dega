PREFIX=/usr/local/pandora/arm-2009q3/usr
OPTFLAGS=-O3 -fomit-frame-pointer -funroll-loops -Wall \
	-ffast-math -mcpu=cortex-a8 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -fsingle-precision-constant
#PREFIX=/usr
#OPTFLAGS=-O3 -fomit-frame-pointer -funroll-loops -Wall
NOPYTHON=1
NOMOVIE=1
Z80=z80jb
#DEBUG=1
PND_FILES = dega dega.png dega.sh dega.txt ggsms.default PXML.xml video.sh

#OPTFLAGS=-O3 -fomit-frame-pointer -funroll-loops -march=i686 -mcpu=i686
#OPTFLAGS=-xM -O3
CC=arm-none-linux-gnueabi-gcc
CXX=arm-none-linux-gnueabi-g++
AS=arm-none-linux-gnueabi-gcc
#CC=gcc
#CXX=g++
#AS=gcc
NASM=nasm
WINDRES=windres

CCVER = $(shell $(CC) -v 2>&1)

ifneq (,$(findstring mingw32,$(CCVER)))
	P=win
else ifneq (,$(findstring cygwin,$(CCVER)))
	P=win
else
	P=unix
endif

ifneq (,$(findstring x86_64,$(CCVER)))
	BITS=64
else
	BITS=32
endif

ifeq ($(P),unix)
	CFLAGS= $(OPTFLAGS) $(shell $(PREFIX)/bin/sdl-config --cflags) -I$(PREFIX)/include -DUSE_MENCODER -Imast -Idoze -Ilibmencoder -D__cdecl= -D__fastcall=
else ifeq ($(P),win)
	CFLAGS= $(OPTFLAGS) -DUSE_VFW -mno-cygwin -Imast -Idoze -Imaster -Iextra -Izlib -Ilibvfw
endif

ASFLAGS = $(OPTFLAGS)

ifdef DEBUG
	CFLAGS += -g
	ASFLAGS += -g
endif

ifndef Z80
	Z80=doze
endif

ifeq ($(Z80),z80jb)
	CFLAGS += -Iz80jb -DEMU_Z80JB
	Z80OBJ = z80jb/z80.o z80jb/z80daisy.o
else
ifeq ($(Z80),drz80)
	CFLAGS += -IDrZ80 -DEMU_DRZ80
	Z80OBJ = DrZ80/DrZ80.o DrZ80/DrZ80Support.o
else
	CFLAGS += -Idoze -DEMU_DOZE
	Z80OBJ = doze/doze.o doze/dozea.o
endif
endif

CXXFLAGS= $(CFLAGS) -fno-exceptions

DAMOBJ = doze/dam.o doze/dama.o doze/damc.o doze/dame.o doze/damf.o doze/damj.o doze/damm.o doze/damo.o doze/damt.o
MASTOBJ = mast/area.o mast/dpsg.o mast/draw.o mast/emu2413.o mast/frame.o mast/load.o mast/map.o mast/mast.o mast/mem.o mast/samp.o mast/snd.o mast/vgm.o mast/video.o mast/osd.o mast/md5.o mast/unzip.o mast/zipfn.o
ifdef $(NOPYTHON)
PYOBJ = python/pydega.o python/stdalone.o
PYEMBOBJ = python/pydega.emb.o python/embed.emb.o
else
CFLAGS += -DNOPYTHON
CXXFLAGS += -DNOPYTHON
endif

CFLAGS += -DFB_RENDER
CXXFLAGS += -DFB_RENDER

ifeq ($(P),unix)
ifeq ($(BITS),64)
	NASM_FORMAT = elf64
else
	NASM_FORMAT = elf
endif
ifeq ($(Z80),drz80)
	EXEEXT = -drz80
else
	EXEEXT =
endif
	SOEXT = .so
	PLATOBJ = sdl/main.o sdl/font.o
	PLATPYOBJ =
	PLATPYOBJCXX =
	EXTRA_LIBS = $(shell $(PREFIX)/bin/sdl-config --libs) -lm -lts -lz -lssl
	DOZE_FIXUP = sed -f doze/doze.cmd.sed <doze/dozea.asm >doze/dozea.asm.new && mv doze/dozea.asm.new doze/dozea.asm
ifndef NOMOVIE
	ENCODER_OBJ = tools/degavi.o
	ENCODER_LIBS = libmencoder/libmencoder.a
	ENCODER_LDFLAGS = -lm
endif
	EXTRA_LDFLAGS =
	GUI_LDFLAGS =
	SPECS =
	PYTHON_CFLAGS = $(shell $(PREFIX)/bin/python-config --cflags) $(CFLAGS)
	PYTHON_CXXFLAGS = $(shell $(PREFIX)/bin/python-config --cflags) $(CXXFLAGS)
	PYTHON_LDFLAGS = $(shell $(PREFIX)/bin/python-config --ldflags) -lm
else ifeq ($(P),win)
	NASM_FORMAT = win32
	EXEEXT = .exe
	SOEXT = .pyd
	PLATOBJ = master/app.o master/conf.o master/dinp.o master/disp.o master/dsound.o master/emu.o master/frame.o master/input.o master/load.o master/loop.o master/main.o master/misc.o master/python.o master/render.o master/run.o master/shot.o master/state.o master/video.o master/zipfn.o master/keymap.o zlib/libz.a
	PLATPYOBJ =
	PLATPYOBJCXX =
	EXTRA_LIBS = -ldsound -ldinput -lddraw -ldxguid -lcomdlg32 -lcomctl32 -luser32 -lwinmm
	DOZE_FIXUP =
	ENCODER_OBJ = tools/wdegavi.o tools/degavirc.o
	ENCODER_LIBS = libvfw/libvfw.a
	ENCODER_LDFLAGS = -lcomdlg32 -lvfw32 -lmsacm32 -lm -Wl,--subsystem,windows
	EXTRA_LDFLAGS = -specs=$(shell pwd)/specs -mno-cygwin
	GUI_LDFLAGS = -Wl,--subsystem,windows
	SPECS = specs
	PYTHON_PREFIX = /home/peter/pytest/winpython
	PYTHON_CFLAGS = -I$(PYTHON_PREFIX)/include $(CFLAGS)
	PYTHON_CXXFLAGS = -I$(PYTHON_PREFIX)/include $(CFLAGS)
	PYTHON_LDFLAGS = -L$(PYTHON_PREFIX)/libs -lpython25
endif

ifdef NOMOVIE
ALLOBJ = dega$(EXEEXT)
else
ALLOBJ = dega$(EXEEXT) mmvconv$(EXEEXT) degavi$(EXEEXT)
endif

ifneq ($(BITS),64)
ifndef NOPYTHON
ALLOBJ += pydega$(SOEXT)
endif
endif

ifeq ($(P),unix)

all: $(ALLOBJ)

release:
	git tag dega-$(R)
	git clone . ../dega-$(R)
	cd ../dega-$(R) && git checkout dega-$(R)
	rm -rf ../dega-$(R)/.git
	git clone ../libmencoder ../dega-$(R)/libmencoder
	rm -rf ../dega-$(R)/libmencoder/.git
	git clone ../libvfw ../dega-$(R)/libvfw
	rm -rf ../dega-$(R)/libvfw/.git
	cd .. && tar czf dega-$(R).tar.gz dega-$(R)

libmencoder/libmencoder.a:
	$(MAKE) -Clibmencoder CFLAGS="$(CFLAGS)" libmencoder.a

else ifeq ($(P),win)

all: $(ALLOBJ)

zlib/libz.a:
	$(MAKE) -Czlib CFLAGS="$(CFLAGS)" libz.a

libvfw/libvfw.a:
	$(MAKE) -Clibvfw CFLAGS="$(CFLAGS)" libvfw.a

release: all
	rm -rf dega-$(R)-win32-$(Z80)
	mkdir dega-$(R)-win32-$(Z80)
	cp dega.exe degavi.exe pydega$(SOEXT) mmvconv.exe dega.txt python/scripts/*.py dega-$(R)-win32-$(Z80)/
	$(STRIP) dega-$(R)-win32-$(Z80)/dega.exe dega-$(R)-win32-$(Z80)/degavi.exe dega-$(R)-win32-$(Z80)/pydega$(SOEXT) dega-$(R)-win32-$(Z80)/mmvconv.exe
	cd dega-$(R)-win32-$(Z80) && zip -9 ../dega-$(R)-win32-$(Z80).zip dega.exe pydega$(SOEXT) degavi.exe mmvconv.exe dega.txt *.py

else

all:
	@echo Supported platforms are unix and win
	@false

endif

dega$(EXEEXT): $(PLATOBJ) $(PLATPYOBJ) $(PLATPYOBJCXX) $(Z80OBJ) $(MASTOBJ) $(PYEMBOBJ) $(SPECS)
	$(CC) $(EXTRA_LDFLAGS) $(GUI_LDFLAGS) -o dega$(EXEEXT) $(PLATOBJ) $(PLATPYOBJ) $(PLATPYOBJCXX) $(Z80OBJ) $(MASTOBJ) $(PYEMBOBJ) $(EXTRA_LIBS)

degavi$(EXEEXT): tools/avioutput.o $(ENCODER_OBJ) $(Z80OBJ) $(MASTOBJ) $(ENCODER_LIBS)
	$(CC) $(EXTRA_LDFLAGS) -o degavi$(EXEEXT) tools/avioutput.o $(ENCODER_OBJ) $(Z80OBJ) $(MASTOBJ) $(ENCODER_LIBS) $(ENCODER_LDFLAGS)

mmvconv$(EXEEXT): tools/mmvconv.o $(SPECS)
	$(CC) $(EXTRA_LDFLAGS) -o mmvconv$(EXEEXT) tools/mmvconv.o

pydega$(SOEXT): $(PYOBJ) $(Z80OBJ) $(MASTOBJ) $(SPECS)
	$(CC) -shared -o pydega$(SOEXT) $(PYOBJ) $(Z80OBJ) $(MASTOBJ) $(EXTRA_LDFLAGS) $(PYTHON_LDFLAGS)

doze/dozea.o: doze/dozea.asm
	nasm -f $(NASM_FORMAT) -o doze/dozea.o doze/dozea.asm

DrZ80/DrZ80.o: DrZ80/DrZ80.s
	$(AS) $(ASFLAGS) -o DrZ80/DrZ80.o -c DrZ80/DrZ80.s

doze/dozea.asm: doze/dam$(EXEEXT)
	cd doze ; $(WINE) ./dam
	$(DOZE_FIXUP)

doze/dam$(EXEEXT): $(DAMOBJ)
	$(CC) -o doze/dam$(EXEEXT) $(DAMOBJ)

master/app.o: master/app.rc
	cd master && $(WINDRES) -o app.o app.rc

tools/degavirc.o: tools/degavirc.rc
	cd tools && $(WINDRES) -o degavirc.o degavirc.rc

specs:
	$(CC) -dumpspecs | sed -e 's/-lmsvcrt/-lmsvcr71/g' > specs

$(PYOBJ): %.o: %.c
	$(CC) -c -o $@ $< $(PYTHON_CFLAGS)

$(PLATPYOBJ): %.o: %.c
	$(CC) -c -o $@ $< -DEMBEDDED $(PYTHON_CFLAGS)

$(PLATPYOBJCXX): %.o: %.cpp
	$(CXX) -c -o $@ $< -DEMBEDDED $(PYTHON_CXXFLAGS)

%.emb.o: %.c
	$(CC) -c -o $@ $< -DEMBEDDED $(PYTHON_CFLAGS)

clean:
	rm -rf $(Z80OBJ) $(DAMOBJ) $(MASTOBJ) $(PLATOBJ) $(PYOBJ) $(PYEMBOBJ) $(PLATPYOBJ) $(PLATPYOBJCXX) tools/degavi.o tools/mmvconv.o doze/dozea.asm* doze/dam doze/dam.exe dega dega.exe degavi degavi.exe mmvconv mmvconv.exe pydega.so pydega.dll pydega.pyd specs dega.squash dega.pnd pnd
	make -Czlib clean

distclean: clean
	rm -f *~ */*~
	
pnd: dega.pnd

dega.pnd: $(PND_FILES)
	rm -rf pnd
	mkdir -p pnd
	cp -f $(PND_FILES) pnd/
	rm -f dega.squash
	mksquashfs pnd dega.squash
	cat dega.squash PXML.xml dega.png > dega.pnd

