CC = sdcc
P18FXXX = 18f2550

LIBDIR = ../libledmtx/lib
INCLUDEDIR = ../libledmtx/include

DRIVER = r393c164
# list of libledmtx objects to link
O = font5x7 stdio
MODULES = scrollstr

OBJECTS = $(DRIVER:%=libledmtx_%.o) $(O:%=$(LIBDIR)/libledmtx_%.o)
LFLAGS = -mpic16 -p$(P18FXXX) -Wl-m -L$(LIBDIR) -l libc18f.lib --use-non-free -l libio$(P18FXXX).lib \
  -l libledmtx_core.lib $(MODULES:%=-l libledmtx_%.lib)
CFLAGS = -mpic16 -p$(P18FXXX) --fomit-frame-pointer --pstack-model=small --use-non-free -I../include \
  -I$(INCLUDEDIR) -DLANG_$(LANG)

OBJECTS += rbuf.o lm35.o main.o
LANG?=EN

all: p18clock.hex

clean:
	rm -f *~ ../include/*~ *.asm *.cod *.hex *.lst *.map *.o

p18clock.hex: $(OBJECTS)
	$(CC) $(LFLAGS) -o $@ $^

$(DRIVER:%=libledmtx_%.o): $(DRIVER:%=ledmtx_%.inc)
	(cd $(LIBDIR)/driver && $(MAKE) D=$(DRIVER) O=$(CURDIR) driver)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
