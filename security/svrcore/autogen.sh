#!/bin/sh

# Check autoconf version
AC_VERSION=`autoconf --version | grep '^autoconf' | sed 's/.*) *//'`
case $AC_VERSION in
'' | 0.* | 1.* | 2.[0-4]* | 2.[0-9] | 2.5[0-8]* )
    echo "You must have autoconf version 2.59 or later installed (found version $AC_VERSION)."
    exit 1
    ;;
* )
    echo "Found autoconf version $AC_VERSION"
    ;;
esac

# Check automake version
AM_VERSION=`automake --version | grep '^automake' | sed 's/.*) *//'`
case $AM_VERSION in
'' | 0.* | 1.[0-8]* | 1.9.[0-1]* )
    echo "You must have automake version 1.9.2 or later installed (found version $AM_VERSION)."
    exit 1
    ;;
* )
    echo "Found automake version $AM_VERSION"
    ;;
esac

# Check libtool version
LT_VERSION=`libtool --version | grep ' libtool)' | sed 's/.*) \([0-9][0-9.]*\)[^ ]* .*/\1/'`
case $LT_VERSION in
'' | 0.* | 1.[0-4]* | 1.5.[0-9] | 1.5.[0-1]* | 1.5.2[0-1]* )
    echo "You must have libtool version 1.5.22 or later installed (found version $LT_VERSION)."
    exit 1
    ;;
* )
    echo "Found libtool version $LT_VERSION"
    ;;
esac

# Run autoreconf
echo "Running autoreconf -fvi"
autoreconf -fvi
