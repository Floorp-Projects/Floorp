#!/bin/sh
# Run this from cron to update the identifier database that lxr uses
# to turn function names into clickable links.
# Created 12-Jun-98 by jwz.

PATH=/opt/local/bin:$PATH
export PATH

lxr_dir=.
db_dir=`sed -n 's@^dbdir:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
log=$db_dir/genxref.log

exec > $log 2>&1
set -x

cd $db_dir/tmp

set -e
time $lxr_dir/genxref $src_dir
chmod -R a+r .
mv xref fileidx ../

exit 0
