#!/bin/sh
# Run this from cron to update the source tree that lxr sees.
# Created 12-Jun-98 by jwz.

CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
export CVSROOT

PATH=/opt/local/bin:/opt/cvs-tools/bin:$PATH
export PATH

lxr_dir=.
db_dir=`sed -n 's@^dbdir:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
log=$db_dir/cvs.log

exec > $log 2>&1
set -x

date

# update the lxr sources
pwd
time cvs -Q -d $CVSROOT update -dP

date

# then update the Mozilla sources
cd $src_dir
cd ..
time cvs -Q -d $CVSROOT checkout MozillaSource

date

exit 0
