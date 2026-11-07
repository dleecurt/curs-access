## Makefile.am - Automake input for Curses Access
## Copyright (C) 2026 Lee Curtis
## This file is free software; see COPYING (GNU GPL v3 or later).

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = curs-access

# Code shared by the program and the tests.
common_sources = \
	src/util.c src/util.h \
	src/signals.c src/signals.h \
	src/crypto.c src/crypto.h \
	src/security.c src/security.h \
	src/theme.c src/theme.h \
	src/prefs.c src/prefs.h \
	src/menu.c src/menu.h \
	src/usage.c src/usage.h

curs_access_SOURCES = $(common_sources) \
	src/main.c \
	src/ui.c src/ui.h \
	src/screens.h \
	src/mainmenu.c \
	src/access.c \
	src/launch.c \
	src/splash.c \
	src/maint.c \
	src/cfgscreen.c

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = $(NCURSES_CFLAGS) $(CRYPTO_CFLAGS) $(WARN_CFLAGS)
curs_access_LDADD = $(NCURSES_LIBS) $(CRYPTO_LIBS)

dist_man_MANS = doc/curs-access.1
dist_doc_DATA = README.md NEWS.md doc/DESIGN.md doc/logo-example.txt

## ---- Tests (make check) -------------------------------------------------
check_PROGRAMS = tests/test_crypto tests/test_files
TESTS = $(check_PROGRAMS)

tests_test_crypto_SOURCES = tests/test_crypto.c $(common_sources)
tests_test_crypto_LDADD = $(NCURSES_LIBS) $(CRYPTO_LIBS)
tests_test_files_SOURCES = tests/test_files.c $(common_sources)
tests_test_files_LDADD = $(NCURSES_LIBS) $(CRYPTO_LIBS)

EXTRA_DIST = autogen.sh .gitignore m4/.gitkeep
