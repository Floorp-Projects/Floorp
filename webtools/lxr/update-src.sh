#!/bin/sh
# Run this from cron to update the source tree that lxr sees.
# Created 12-Jun-98 by jwz.

CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
export CVSROOT

PATH=/opt/local/bin:$PATH
export PATH

lxr_dir=.
src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`

cd $src_dir
cd ..
cvs -Q -d $CVSROOT checkout MozillaSource
exit 0
