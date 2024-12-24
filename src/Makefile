CC = sdcc
P18FXXX = 18f2550

# 32x7 display @ 50 Hz (given a Fosc of 8 MHz for the primary oscillator)
OSC_HZ = 8000000
VIDEO_MODE_DESC = 32 7 7 50

# Set `INSTALL_PREFIX` to actual libledmtx installation directory.
INSTALL_PREFIX = $(HOME)/.local/libledmtx/

LIBDIR = $(INSTALL_PREFIX)/lib/
INCLUDEDIR = $(INSTALL_PREFIX)/include/
BINDIR = $(INSTALL_PREFIX)/bin/

DRIVER = r393c164
# list of libledmtx objects to link
O = font5x7 stdio
MODULES = scrollstr

OBJECTS = $(DRIVER:%=libledmtx_%.o) $(O:%=$(LIBDIR)/libledmtx_%.o)
LFLAGS = -mpic16 -p$(P18FXXX) -Wl-m -L$(LIBDIR) -l libc18f.lib --use-non-free -l libio$(P18FXXX).lib \
  -l libledmtx_core.lib $(MODULES:%=-l libledmtx_%.lib)
CFLAGS = -mpic16 -p$(P18FXXX) --fomit-frame-pointer --pstack-model=small --use-non-free -I../include \
  -I$(INCLUDEDIR) -DLANG_$(P18CLOCK_LANG)

OBJECTS += rbuf.o lm35.o main.o
P18CLOCK_LANG?=EN

all: ledmtx_modegen_modes.h p18clock.hex

clean:
	rm -f *~ ../include/*~ *.asm *.cod *.hex *.lst *.map *.o ledmtx_modegen_modes.h

ledmtx_modegen_modes.h:
	$(BINDIR)/ledmtx_modegen --osc-hz $(OSC_HZ) --default=0 $(VIDEO_MODE_DESC) > $@

p18clock.hex: $(OBJECTS)
	$(CC) $(LFLAGS) -o $@ $^

$(DRIVER:%=libledmtx_%.o): $(DRIVER:%=ledmtx_%.inc)
	(cd $(LIBDIR)/driver && $(MAKE) D=$(DRIVER) O=$(CURDIR) driver)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
