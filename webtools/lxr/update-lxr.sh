#!/bin/sh
# Run this from cron to update the glimpse database that lxr uses
# to do full-text searches.
# Created 12-Jun-98 by jwz.
# Updated 2-27-99 by endico. Added multiple tree support.
# Updated 3-6-99 by endico. Combine src, xref and source files 
#    together into one script.

CVSROOT=/cvsroot
export CVSROOT

PATH=/opt/local/bin:/opt/cvs-tools/bin:/usr/ucb:$PATH
export PATH

TREE=$1
export TREE

lxr_dir=`pwd`
echo $lxr_dir
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

if [ -f $db_dir/update.log ]
  then
  mv $db_dir/update.log $db_dir/update.log.0
  fi

log=$db_dir/update.log

exec > $log 2>&1
set -x

date

#
# update the lxr sources
#
pwd
time cvs -d $CVSROOT update -dP

date

#
# then update the Mozilla sources
#
cd $src_dir
cd ..

case "$1" in

'classic')
    time cvs -Q -d $CVSROOT checkout -P -rMozillaSourceClassic_19981026_BRANCH MozillaSource
    ;;
'ef')
    time cvs -Q -d $CVSROOT checkout -P mozilla/ef
    time cvs -Q -d $CVSROOT checkout -P mozilla/nsprpub
    ;;
'grendel')
    time cvs -Q -d $CVSROOT checkout -P Grendel
    ;;
'mailnews')
    time cvs -Q -d $CVSROOT checkout -P SeaMonkeyMailNews
    ;;
'mozilla')
    time cvs -Q -d $CVSROOT checkout -P mozilla
    ;;
'nspr')
    time cvs -Q -d $CVSROOT checkout -P NSPR
    ;;
'seamonkey')
    time cvs -Q -d $CVSROOT checkout -P SeaMonkeyAll
    ;;
'gnome')
    CVSROOT=/cvs/gnome
    export CVSROOT
    cd $src_dir
    time cvs -Q -d $CVSROOT checkout -P glib
    time cvs -Q -d $CVSROOT checkout -P gtk+
    cd ..
    ;;
esac


date
uptime

#
# generate cross reference database
#
cd $db_dir/tmp

set -e
time $lxr_dir/genxref $src_dir
chmod -R a+r .
mv xref fileidx ../

date
uptime


#
# generate glimpse index
#
cd $db_dir/tmp

# don't index CVS files
echo '/CVS/' > .glimpse_exclude

set -e
time $lxr_dir/glimpseindex -H . $src_dir
chmod -R a+r .
mv .glimpse* ../

date
uptime

exit 0
