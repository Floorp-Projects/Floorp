#!/bin/sh

set -v -e -x

if [ $# -lt 1 ]; then
  echo "Usage: update-libjpeg.sh /path/to/libjpeg-turbo/ [tag]"
  exit 1
fi

srcdir=`realpath $(dirname $0)`
topsrcdir=${srcdir}/..
rm -rf $srcdir/libjpeg

repo=$1
tag=${2-HEAD}

(cd $repo; git archive --prefix=media/libjpeg/ $tag) | (cd $srcdir/..; tar xf -)

cd $srcdir/libjpeg
cp win/jsimdcfg.inc simd/

revert_files="1050342.diff jconfig.h jconfigint.h moz.build MOZCHANGES mozilla.diff simd/jsimdcfg.inc"
if test -d ${topsrcdir}/.hg; then
    hg revert --no-backup $revert_files
elif test -d ${topsrcdir}/.git; then
    git checkout HEAD -- $revert_files
fi

patch -p0 -i mozilla.diff
patch -p0 -i 1050342.diff
