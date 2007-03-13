#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(svrcore, 4.0.4, rmeggins@redhat.com, svrcore)
AM_INIT_AUTOMAKE([gnu check-news dist-bzip2])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/svrcore.h])
AC_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([string.h],,[AC_ERROR([We need string.h in order to build])])
AC_CHECK_HEADER([termios.h],,[AC_ERROR([We need termios.h in order to build])])
AC_CHECK_HEADER([unistd.h],,[AC_ERROR([We need unistd.h in order to build])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strchr strdup])

# Check for library dependencies
m4_include(m4/nss.m4)
m4_include(m4/nspr.m4)

AC_SUBST([NSPR_CFLAGS])
AC_SUBST([NSPR_LIBS])
AC_SUBST([NSPR_MIN_VER])
AC_SUBST([NSPR_NAME])
AC_SUBST([NSS_CFLAGS])
AC_SUBST([NSS_LIBS])
AC_SUBST([NSS_MIN_VER])
AC_SUBST([NSS_NAME])

AC_CONFIG_FILES([src/Makefile Makefile svrcore.pc])
AC_OUTPUT
