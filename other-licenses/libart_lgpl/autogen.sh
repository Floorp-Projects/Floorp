#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# This was lifted from the Gimp, and adapted slightly by
# Raph Levien <raph@acm.org>.

DIE=0
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PROJECT=LibArt_LGPL

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

# Do we really need libtool?
(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $PROJECT."
	echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.2.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

case $CC in
xlc )
    am_opt=--include-deps;;
esac

for dir in $srcdir
do 
  echo processing $dir
  (cd $dir; \
  aclocalinclude="$ACLOCAL_FLAGS"; \
  aclocal $aclocalinclude; \
  autoheader; automake --add-missing --gnu $am_opt; autoconf)
done

$srcdir/configure "$@"

echo 
echo "Now type 'make' to compile $PROJECT."
