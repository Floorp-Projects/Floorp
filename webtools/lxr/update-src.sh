#!/bin/sh
# Run this from cron to update the source tree that lxr sees.
# Created 12-Jun-98 by jwz.

CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
export CVSROOT

PATH=/opt/local/bin:$PATH
export PATH

lxr_dir=.
db_dir=`sed -n 's@^dbdir:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
log=$db_dir/cvs.log

exec > $log 2>&1
set -x

# update the lxr sources
pwd
cvs -Q -d $CVSROOT update -dP

# then update the Mozilla sources
cd $src_dir
cd ..
cvs -Q -d $CVSROOT checkout MozillaSource

exit 0
