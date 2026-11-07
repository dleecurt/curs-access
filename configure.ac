dnl configure.ac - Autoconf input for Curses Access
dnl Copyright (C) 2026 Lee Curtis
dnl This file is free software; see COPYING (GNU GPL v3 or later).

AC_PREREQ([2.69])
AC_INIT([Curses Access], [0.0.2], [], [curs-access])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
PKG_PROG_PKG_CONFIG

dnl ---- ncurses (wide character version) --------------------------------
PKG_CHECK_MODULES([NCURSES], [ncursesw], [],
  [AC_MSG_ERROR([ncursesw was not found (Debian: apt install libncurses-dev)])])

dnl ---- Encryption: libsodium preferred, OpenSSL 3 as the fallback ------
AC_ARG_WITH([crypto],
  [AS_HELP_STRING([--with-crypto=auto|sodium|openssl],
    [encryption library (default: auto = libsodium if found, else OpenSSL)])],
  [], [with_crypto=auto])

crypto_backend=none
AS_CASE([$with_crypto],
  [auto|sodium],
    [PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.15],
       [crypto_backend=libsodium], [crypto_backend=none])],
  [openssl], [],
  [AC_MSG_ERROR([--with-crypto must be auto, sodium or openssl])])

AS_IF([test "x$with_crypto" = xsodium && test "x$crypto_backend" != xlibsodium],
  [AC_MSG_ERROR([libsodium was not found (Debian: apt install libsodium-dev)])])

AS_IF([test "x$crypto_backend" = xlibsodium],
  [AC_DEFINE([HAVE_LIBSODIUM], [1], [Define to 1 to use libsodium])
   CRYPTO_CFLAGS="$SODIUM_CFLAGS"
   CRYPTO_LIBS="$SODIUM_LIBS"],
  [PKG_CHECK_MODULES([OPENSSL], [libcrypto >= 3.0],
     [crypto_backend="OpenSSL (scrypt, AES-256-GCM, X25519)"
      CRYPTO_CFLAGS="$OPENSSL_CFLAGS"
      CRYPTO_LIBS="$OPENSSL_LIBS"],
     [AC_MSG_ERROR([neither libsodium nor OpenSSL 3 was found
  (Debian: apt install libsodium-dev, or libssl-dev)])])])
AC_SUBST([CRYPTO_CFLAGS])
AC_SUBST([CRYPTO_LIBS])

dnl ---- Compiler warnings ------------------------------------------------
WARN_CFLAGS="-Wall -Wextra -Wno-unused-parameter"
AC_SUBST([WARN_CFLAGS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_NOTICE([
  Curses Access $PACKAGE_VERSION
    prefix:      $prefix
    encryption:  $crypto_backend
])
