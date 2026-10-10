# Build for dyck.
#
#   make                 build ./dyck
#   make test            build, then run tests/run.sh
#   make bench           time the count of every word for n = 14 to 18
#   make install         install into $(DESTDIR)$(PREFIX)/bin
#   make clean           remove build/ and the binary
#   make CC=gcc OPT=-O0  any of the variables, from the command line

CC       ?= cc
CSTD     ?= -std=c99
WARNINGS ?= -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wshadow -Wconversion
OPT      ?= -O2
CFLAGS   ?= $(CSTD) $(WARNINGS) $(OPT)

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DESTDIR ?=

BIN     := dyck
SRCDIR  := src
OBJDIR  := build
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS    := $(OBJECTS:.o=.d)

.PHONY: all test bench install uninstall clean help
.DEFAULT_GOAL := all

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

# -MMD -MP writes build/<name>.d beside each object, so touching a header
# rebuilds what includes it. Understood by both gcc and clang.
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: $(BIN)
	./tests/run.sh ./$(BIN)

bench: $(BIN)
	./tests/bench.sh ./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -rf $(OBJDIR) $(BIN)

help:
	@echo 'Targets: all test bench install uninstall clean'
	@echo 'Variables: CC CFLAGS CSTD WARNINGS OPT LDFLAGS LDLIBS PREFIX BINDIR DESTDIR'

-include $(DEPS)
