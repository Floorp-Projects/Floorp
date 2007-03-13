# -*- tab-width: 4; -*-
# Configure paths for NSPR
# Public domain - Rich Megginson <richm@stanfordalumni.org> 2007-03-08
# Based on other Mozilla m4 work by Chris Seawood <cls@seawood.org> 2001-04-05

AC_CHECKING(for NSS)

# check for --with-nss
AC_MSG_CHECKING(for --with-nss)
AC_ARG_WITH(nss, [  --with-nss=PATH         Network Security Services (NSS) directory],
[
  if test -e "$withval"/include/nss.h -a -d "$withval"/lib
  then
    AC_MSG_RESULT([using $withval])
    NSSDIR=$withval
    nss_inc="-I$NSSDIR/include"
    nss_lib="-L$NSSDIR/lib"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# check for --with-nss-inc
AC_MSG_CHECKING(for --with-nss-inc)
AC_ARG_WITH(nss-inc, [  --with-nss-inc=PATH         Network Security Services (NSS) include directory],
[
  if test -e "$withval"/nss.h
  then
    AC_MSG_RESULT([using $withval])
    nss_inc="-I$withval"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# check for --with-nss-lib
AC_MSG_CHECKING(for --with-nss-lib)
AC_ARG_WITH(nss-lib, [  --with-nss-lib=PATH         Network Security Services (NSS) library directory],
[
  if test -d "$withval"
  then
    AC_MSG_RESULT([using $withval])
    nss_lib="-L$withval"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# see if we are building "in tree" with the
# other mozilla components
if test -z "$nss_inc" -o -z "$nss_lib"; then
    nsslibpath=`echo ../../dist/*.OBJ/lib | cut -f1 -d' '`
    savedir=`pwd`
    cd $nsslibpath
    abs_nsslibpath=`pwd`
    cd $savedir
    nssincpath=../../dist/public/nss
    savedir=`pwd`
    cd $nssincpath
    abs_nssincpath=`pwd`
    cd $savedir
    if test -f "$abs_nssincpath/nss.h" ; then
        nss_inc="-I$abs_nssincpath"
    fi
    if test -d "$abs_nsslibpath" ; then
        nss_lib="-L$abs_nsslibpath"
    fi
    if test -n "$nss_inc" -a -n "$nss_lib" ; then
        AC_MSG_CHECKING(using in-tree NSS from $nssincpath $nsslibpath)
    else
        AC_MSG_CHECKING(could not find in-tree NSS in ../../dist)
    fi
fi

# if NSS is not found yet, try pkg-config
# last resort
if test -z "$nss_inc" -o -z "$nss_lib"; then
  AC_MSG_CHECKING(for nss with pkg-config)
  AC_PATH_PROG(PKG_CONFIG, pkg-config)
  if test -n "$PKG_CONFIG"; then
    if $PKG_CONFIG --exists nss; then
      nss_inc=`$PKG_CONFIG --cflags-only-I nss`
      nss_lib=`$PKG_CONFIG --libs-only-L nss`
      nss_ver=`$PKG_CONFIG --modversion nss`
      nss_name=nss
      AC_MSG_RESULT([using system NSS])
    elif $PKG_CONFIG --exists dirsec-nss; then
      nss_inc=`$PKG_CONFIG --cflags-only-I dirsec-nss`
      nss_lib=`$PKG_CONFIG --libs-only-L dirsec-nss`
      nss_ver=`$PKG_CONFIG --modversion dirsec-nss`
      nss_name=dirsec-nss
      AC_MSG_RESULT([using system dirsec NSS])
    else
      AC_MSG_ERROR([NSS not found, specify with --with-nss.])
    fi
  fi
fi

NSS_CFLAGS="$nss_inc"
NSS_LIBS="$nss_lib -lssl3 -lnss3"
if test -z "$nss_ver" ; then
	nss_ver=3.11.4
fi
NSS_MIN_VER="$nss_ver"
if test -z "$nss_name" ; then
	nss_name=nss
fi
NSS_NAME="$nss_name"
