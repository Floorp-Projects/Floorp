#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# assume $1 is the directory with a git checkout of the libsrtp source
#
# srtp source is available at: https://github.com/cisco/libsrtp/
#
# also assumes we've made *NO* changes to the SRTP sources!  If we do, we have to merge by
# hand after this process, or use a more complex one.
#
# For example, one could update an srtp library import head, and merge back to default.  Or keep a
# separate repo with this in it, and pull from there to m-c and merge.
if [ "$1" ] ; then
  DATE=`date`
  REVISION=`(cd $1; git log --pretty=oneline | head -1 | cut -c 1-40)`
  cp -rf $1/srtp $1/crypto $1/include $1/test $1/LICENSE $1/CHANGES $1/README.md third_party/libsrtp/src

  hg addremove third_party/libsrtp/src --include "third_party/libsrtp/src/VERSION" --include "third_party/libsrtp/src/LICENSE" --include "third_party/libsrtp/src/README.md" --include "**.c" --include "**.h" --similarity 90

  echo "srtp updated to revision $REVISION from git on $DATE" >> third_party/libsrtp/srtp_update.log
  echo "srtp updated to revision $REVISION from git on $DATE"
  echo "WARNING: reapply any local patches!"
else
  echo "usage: $0 path_to_srtp_directory"
  echo "run from the root of your m-c clone"
fi
