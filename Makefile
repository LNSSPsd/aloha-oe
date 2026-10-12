# See INSTALL for build and installation instructions.

CC = cc
CFLAGS = -O2 -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS =
LDFLAGS =
LDLIBS = -lm
PROGRAM = aloha-oe
SYSTEM = auto

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
DESTDIR =
INSTALL = install
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

TAR = tar
DISTNAME = aloha-oe
DISTFILE = $(DISTNAME).tar.gz

OBJS = src/main.o src/options.o src/score.o src/synth.o src/phoneme.o \
	src/audio/pcm.o src/audio/darwin.o src/audio/oss.o src/audio/linux.o \
	src/audio/winmm.o src/audio/netbsd.o src/audio/openbsd.o \
	src/audio/null.o

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	@system='$(SYSTEM)'; \
	if test "$$system" = auto; then system=$$(uname -s); fi; \
	case "$$system" in \
	  Darwin) native='-framework AudioToolbox -framework CoreFoundation' ;; \
	  Windows_NT|MINGW*|MSYS*) native='-municode -lwinmm' ;; \
	  *) native='' ;; \
	esac; \
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) $$native

src/main.o: src/main.c src/options.h src/score.h src/synth.h \
	src/phoneme.h src/audio.h
src/options.o: src/options.c src/options.h src/score.h src/synth.h \
	src/phoneme.h
src/score.o: src/score.c src/score.h src/synth.h src/melody.h
src/synth.o: src/synth.c src/synth.h src/phoneme.h
src/phoneme.o: src/phoneme.c src/phoneme.h
src/audio/pcm.o src/audio/darwin.o src/audio/oss.o src/audio/linux.o \
src/audio/winmm.o src/audio/netbsd.o src/audio/openbsd.o \
src/audio/null.o: src/audio.h src/audio/pcm.h

check: $(PROGRAM)
	./$(PROGRAM) --check

preview: $(PROGRAM)
	./$(PROGRAM) --section chorus --seconds 16

install: $(PROGRAM)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man6"
	$(INSTALL_PROGRAM) $(PROGRAM) "$(DESTDIR)$(BINDIR)/$(PROGRAM)"
	$(INSTALL_DATA) doc/aloha-oe.6 "$(DESTDIR)$(MANDIR)/man6/aloha-oe.6"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(PROGRAM)" \
	    "$(DESTDIR)$(MANDIR)/man6/aloha-oe.6"

# Build the archive elsewhere, then replace the old one on success.
dist: MANIFEST
	@set -e; \
	COPYFILE_DISABLE=1; export COPYFILE_DISABLE; \
	case '$(DISTNAME)' in \
	  ''|.|..|*[!A-Za-z0-9._-]*) echo 'Invalid DISTNAME' >&2; exit 1 ;; \
	esac; \
	case "$$($(TAR) --version)" in \
	  *bsdtar*) tarflags='--uid=0 --gid=0 --numeric-owner --no-xattrs --no-acls --no-fflags' ;; \
	  *'GNU tar'*) tarflags='--owner=0 --group=0 --numeric-owner --no-xattrs --no-acls' ;; \
	  *) echo 'make dist requires GNU tar or bsdtar' >&2; exit 1 ;; \
	esac; \
	stage=$$(mktemp -d "$${TMPDIR:-/tmp}/aloha-oe.XXXXXXXX"); \
	trap 'rm -rf "$$stage"' 0; \
	trap 'exit 1' 1 2 3 15; \
	mkdir "$$stage/$(DISTNAME)"; \
	$(TAR) --format=ustar $$tarflags -cf "$$stage/source.tar" -T MANIFEST; \
	$(TAR) -xf "$$stage/source.tar" -C "$$stage/$(DISTNAME)"; \
	$(TAR) --format=ustar $$tarflags -czf "$$stage/dist.tar.gz" -C "$$stage" '$(DISTNAME)'; \
	mv -f "$$stage/dist.tar.gz" '$(DISTFILE)'; \
	echo '$(DISTFILE)'

clean:
	rm -f $(PROGRAM) $(OBJS)

.PHONY: all check preview install uninstall dist clean
