#!/bin/sh
# Run this from cron to update the glimpse database that lxr uses
# to do full-text searches.
# Created 12-Jun-98 by jwz.

PATH=/opt/local/bin:/opt/cvs-tools/bin:$PATH
export PATH

lxr_dir=.
db_dir=`sed -n 's@^dbdir:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
log=$db_dir/glimpseindex.log

exec > $log 2>&1
set -x

date

cd $db_dir/tmp

set -e
time glimpseindex -H . $src_dir
chmod -R a+r .
mv .glimpse* ../

date

exit 0
