#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# assume $1 is the directory with a SVN checkout of the libsctp source
#
# sctp usrlib source is available (via svn) at:
# svn co https://sctp-refimpl.googlecode.com/svn/trunk sctpSVN
#
# also assumes we've made *NO* changes to the SCTP sources!  If we do, we have to merge by
# hand after this process, or use a more complex one.
#
# For example, one could update an sctp library import head, and merge back to default.  Or keep a
# separate repo with this in it, and pull from there to m-c and merge.
if [ "$1" ] ; then
  export date=`date`
  export revision=`(cd $1; svnversion -n)`

  cp $1/KERN/usrsctp/usrsctplib/*.c $1/KERN/usrsctp/usrsctplib/*.h netwerk/sctp/src
  cp $1/KERN/usrsctp/usrsctplib/netinet/*.c $1/KERN/usrsctp/usrsctplib/netinet/*.h netwerk/sctp/src/netinet
  cp $1/KERN/usrsctp/usrsctplib/netinet6/*.c $1/KERN/usrsctp/usrsctplib/netinet6/*.h netwerk/sctp/src/netinet6

  hg addremove netwerk/sctp/src --include "**.c" --include "**.h" --similarity 90

  echo "sctp updated to version $revision from SVN on $date" >> netwerk/sctp/sctp_update.log
  echo "sctp updated to version $revision from SVN on $date"
  echo "WARNING: reapply any local patches!"
else
  echo "usage: $0 path_to_sctpSVN_directory"
  echo "run from the root of your m-c clone"
fi
