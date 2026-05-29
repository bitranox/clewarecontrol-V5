# cleware_temp — Makefile
#
# Build:    make
# Install:  sudo make install            (binary -> $(PREFIX)/bin)
# Remove:   sudo make uninstall
#
# Honors DESTDIR and PREFIX for packaging, e.g.:
#   make install DESTDIR=/tmp/pkg PREFIX=/usr

VERSION  := 1.0.0
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

CC       ?= cc
PKGS     := hidapi-libusb
CFLAGS   ?= -O2 -Wall -Wextra
CFLAGS   += -DCLEWARE_TEMP_VERSION=\"$(VERSION)\" $(shell pkg-config --cflags $(PKGS))
LDLIBS   += $(shell pkg-config --libs $(PKGS))

BIN      := cleware_temp

.PHONY: all clean install uninstall test

all: $(BIN)

$(BIN): cleware_temp.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f $(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

# quick smoke test against an attached sensor
test: $(BIN)
	./$(BIN) --plain
