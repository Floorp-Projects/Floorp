#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is SniffURI.
#
# The Initial Developer of the Original Code is
# Erik van der Poel <erik@vanderpoel.org>.
# Portions created by the Initial Developer are Copyright (C) 1998-2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Bruce Robson <bns_robson@hotmail.com>
#
# ***** END LICENSE BLOCK *****

AC_PREREQ(2.59)
AC_INIT(SniffURI, 0.1, erik@vanderpoel.org)
AC_CONFIG_SRCDIR([thread.h])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `XXX' with a function in `-lnsl':
AC_CHECK_LIB([nsl], [XXX])
AC_CHECK_LIB([pthread], [pthread_create])
# FIXME: Replace `XXX' with a function in `-lsocket':
AC_CHECK_LIB([socket], [XXX])
AC_CHECK_LIB([thread], [thr_create])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([gethostbyname gethostbyname_r gethostname gettimeofday memset mkdir select socket strcasecmp strchr strdup strerror strrchr strstr sysinfo])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
