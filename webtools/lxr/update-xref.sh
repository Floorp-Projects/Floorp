#!/bin/sh
# Run this from cron to update the identifier database that lxr uses
# to turn function names into clickable links.
# Created 12-Jun-98 by jwz.
# Updated 27-Feb-99 by endico. Added multiple tree support.

PATH=/opt/local/bin:/opt/cvs-tools/bin:$PATH
export PATH

TREE=$1
export TREE

lxr_dir=.
db_dir=`sed -n 's@^dbdir:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`/$TREE

if [ "$TREE" = '' ]
then
    #since no tree is defined, assume sourceroot is defined the old way
    #grab sourceroot from config file indexing only a single tree where
    #format is "sourceroot: dirname"
    src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`

else
    #grab sourceroot from config file indexing multiple trees where
    #format is "sourceroot: treename dirname"
    src_dir=`sed -n 's@^sourceroot:[ 	]*\(.*\)@\1@p' < $lxr_dir/lxr.conf | grep $TREE | sed -n "s@^$TREE \(.*\)@\1@p"`
fi

log=$db_dir/genxref.log

exec > $log 2>&1
set -x

date

lxr_dir=`pwd`
cd $db_dir/tmp

set -e
time $lxr_dir/genxref $src_dir
chmod -R a+r .
mv xref fileidx ../

date
uptime

exit 0
