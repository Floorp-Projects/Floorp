#!/bin/sh
# Run this from cron to update the glimpse database that lxr uses
# to do full-text searches.
# Created 12-Jun-98 by jwz.
# Updated 2-27-99 by endico. Added multiple tree support.

PATH=/opt/local/bin:/opt/cvs-tools/bin:/usr/ucb:$PATH
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
    src_dir=`sed -n 's@^sourceroot:[    ]*\(.*\)@\1@p' < $lxr_dir/lxr.conf`
 
else
    #grab sourceroot from config file indexing multiple trees where
    #format is "sourceroot: treename dirname"
    src_dir=`sed -n 's@^sourceroot:[    ]*\(.*\)@\1@p' < $lxr_dir/lxr.conf | grep $TREE | sed -n "s@^$TREE \(.*\)@\1@p"`
fi 

log=$db_dir/glimpseindex.log

exec > $log 2>&1
set -x

date

cd $db_dir/tmp

# don't index CVS files
echo '/CVS/' > .glimpse_exclude

set -e
time glimpseindex -H . $src_dir
chmod -R a+r .
mv .glimpse* ../

date
uptime

exit 0
