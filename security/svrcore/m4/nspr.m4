# -*- tab-width: 4; -*-
# Configure paths for NSPR
# Public domain - Rich Megginson <richm@stanfordalumni.org> 2007-03-08
# Based on other Mozilla m4 work by Chris Seawood <cls@seawood.org> 2001-04-05

AC_CHECKING(for NSPR)

# check for --with-nspr
AC_MSG_CHECKING(for --with-nspr)
AC_ARG_WITH(nspr, [  --with-nspr=PATH        Netscape Portable Runtime (NSPR) directory],
[
  if test -e "$withval"/include/nspr.h -a -d "$withval"/lib
  then
    AC_MSG_RESULT([using $withval])
    NSPRDIR=$withval
    nspr_inc="-I$NSPRDIR/include"
    nspr_lib="-L$NSPRDIR/lib"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# check for --with-nspr-inc
AC_MSG_CHECKING(for --with-nspr-inc)
AC_ARG_WITH(nspr-inc, [  --with-nspr-inc=PATH        Netscape Portable Runtime (NSPR) include file directory],
[
  if test -e "$withval"/nspr.h
  then
    AC_MSG_RESULT([using $withval])
    nspr_inc="-I$withval"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# check for --with-nspr-lib
AC_MSG_CHECKING(for --with-nspr-lib)
AC_ARG_WITH(nspr-lib, [  --with-nspr-lib=PATH        Netscape Portable Runtime (NSPR) library directory],
[
  if test -d "$withval"
  then
    AC_MSG_RESULT([using $withval])
    nspr_lib="-L$withval"
  else
    echo
    AC_MSG_ERROR([$withval not found])
  fi
],
AC_MSG_RESULT(no))

# see if we are doing an "in-tree" build with the other
# mozilla components
if test -z "$nspr_inc" -o -z "$nspr_lib" ; then
    for nsprpath in "../../dist" "../../dist/*.OBJ" ; do
        savedir=`pwd`
        cd $nsprpath
        abs_nsprpath=`pwd`
        cd $savedir
        if test -f "$abs_nsprpath/include/nspr/nspr.h" ; then
            nspr_inc="-I$abs_nsprpath/include/nspr"
        elif test -f "$abs_nsprpath/include/nspr.h" ; then
            nspr_inc="-I$abs_nsprpath/include"
        fi
        if test -d "$abs_nsprpath/lib" ; then
            nspr_lib="-L$abs_nsprpath/lib"
        fi
        if test -n "$nspr_inc" -a -n "$nspr_lib" ; then
            break
        fi
    done
fi

# if NSPR is not found yet, try pkg-config
# last resort
if test -z "$nspr_inc" -o -z "$nspr_lib" ; then
  AC_MSG_CHECKING(for nspr with pkg-config)
  AC_PATH_PROG(PKG_CONFIG, pkg-config)
  if test -n "$PKG_CONFIG"; then
    if $PKG_CONFIG --exists nspr; then
      nspr_inc=`$PKG_CONFIG --cflags-only-I nspr`
      nspr_lib=`$PKG_CONFIG --libs-only-L nspr`
      nspr_ver=`$PKG_CONFIG --modversion nspr`
      nspr_name=nspr
      AC_MSG_RESULT([using system NSPR])
    elif $PKG_CONFIG --exists dirsec-nspr; then
      nspr_inc=`$PKG_CONFIG --cflags-only-I dirsec-nspr`
      nspr_lib=`$PKG_CONFIG --libs-only-L dirsec-nspr`
      nspr_ver=`$PKG_CONFIG --modversion dirsec-nspr`
      nspr_name=dirsec-nspr
      AC_MSG_RESULT([using system dirsec NSPR])
    else
      AC_MSG_ERROR([NSPR not found, specify with --with-nspr.])
    fi
  fi
fi

NSPR_CFLAGS="$nspr_inc"
NSPR_LIBS="$nspr_lib -lplds4 -lplc4 -lnspr4"
if test -z "$nspr_ver" ; then
	nspr_ver=4.6.4
fi
NSPR_MIN_VER="$nspr_ver"
if test -z "$nspr_name" ; then
	nspr_name=nspr
fi
NSPR_NAME="$nspr_name"
