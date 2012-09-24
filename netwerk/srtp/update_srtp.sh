#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# assume $1 is the directory with a SVN checkout of the libsrtp source
#
# srtp source is available (via cvs) at:
# cvs -d:pserver:anonymous@srtp.cvs.sourceforge.net:/cvsroot/srtp login
# cvs -z3 -d:pserver:anonymous@srtp.cvs.sourceforge.net:/cvsroot/srtp co -P srtp
#
# also assumes we've made *NO* changes to the SRTP sources!  If we do, we have to merge by
# hand after this process, or use a more complex one.
#
# For example, one could update an srtp library import head, and merge back to default.  Or keep a
# separate repo with this in it, and pull from there to m-c and merge.
if [ "$1" ] ; then
  export date=`date`
#  export revision=`(cd $1; svnversion -n)`
  export CVSREAD=0
  cp -rf $1/srtp $1/crypto $1/include $1/VERSION $1/LICENSE $1/README $1/configure.in netwerk/srtp/src

  hg addremove netwerk/srtp/src --include "netwerk/srtp/src/VERSION" --include "netwerk/srtp/src/LICENSE" --include "netwerk/srtp/src/configure.in" --include "netwerk/srtp/src/README" --include "**.c" --include "**.h" --similarity 90

  echo "srtp updated from CVS on $date" >> netwerk/srtp/srtp_update.log
  echo "srtp updated from CVS on $date"
  echo "WARNING: reapply any local patches!"
else
  echo "usage: $0 path_to_srtp_directory"
  echo "run from the root of your m-c clone"
fi
