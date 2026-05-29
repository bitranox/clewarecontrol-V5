# cleware_temp — Makefile
#
# Build:    make
# Test:     make check                    (unit tests, no hardware needed)
# Install:  sudo make install             (binary -> $(PREFIX)/bin)
# Remove:   sudo make uninstall
#
# Honors DESTDIR and PREFIX for packaging, e.g.:
#   make install DESTDIR=/tmp/pkg PREFIX=/usr
# Add flags without clobbering the defaults, e.g. in CI:
#   make EXTRA_CFLAGS=-Werror

VERSION  := 1.0.1
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

CC       ?= cc
PKGS     := hidapi-libusb
WARNINGS ?= -O2 -Wall -Wextra
CFLAGS   += $(WARNINGS) $(EXTRA_CFLAGS) -DCLEWARE_TEMP_VERSION=\"$(VERSION)\" \
            $(shell pkg-config --cflags $(PKGS))
LDLIBS   += $(shell pkg-config --libs $(PKGS))

# the unit tests are pure (no hidapi) so they compile/link standalone
TESTCFLAGS ?= -O2 -Wall -Wextra $(EXTRA_CFLAGS)

BIN      := cleware_temp

.PHONY: all clean check install uninstall test

all: $(BIN)

$(BIN): cleware_temp.c cleware_decode.h cleware_serial.h
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

# unit tests — no sensor required, runs anywhere (used by CI)
check: tests/test_decode.c tests/test_serial.c cleware_decode.h cleware_serial.h
	$(CC) $(TESTCFLAGS) tests/test_decode.c -o tests/test_decode
	./tests/test_decode
	$(CC) $(TESTCFLAGS) tests/test_serial.c -o tests/test_serial
	./tests/test_serial

clean:
	rm -f $(BIN) tests/test_decode tests/test_serial

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

# quick smoke test against an attached sensor (needs the hardware)
test: $(BIN)
	./$(BIN) --plain
