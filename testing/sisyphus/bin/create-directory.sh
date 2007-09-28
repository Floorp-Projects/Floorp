#!/usr/local/bin/bash -e
# -*- Mode: Shell-script; tab-width: 4; indent-tabs-mode: nil; -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006.
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Bob Clary <bob@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

TEST_DIR=${TEST_DIR:-/work/mozilla/mozilla.com/test.mozilla.com/www}
TEST_BIN=${TEST_BIN:-$TEST_DIR/bin}
source ${TEST_BIN}/library.sh

#
# options processing
#
options="d:n"
function usage()
{
    cat <<EOF
usage: 
$SCRIPT -d directory [-n] 

-d directory    directory to be created.
-n              never prompt when removing existing directory.

Attempts to safely create an empty directory. If -n is not
specified, the script will prompt before deleting any files 
or directories. If -n is specified, it will not prompt.

The safety measures include refusing to run if run by user
root and by refusing to create directories unless there are 
a subdirectory of /tmp or have at least two ancestor 
directories... /grandparent/parent/child.

******************** WARNING ********************
This script will destroy existing directories and
their contents. It can potentially wipe out your
disk. Use with caution.
    ******************** WARNING ********************

EOF
    exit 1
}

unset directory

rmopt="-i"

while getopts $options optname ; 
  do 
  case $optname in
      d) directory=$OPTARG;;
      n) unset rmopt;;
  esac
done

if [[ -z $directory ]]
    then
    usage
fi

if [[ `whoami` == "root" ]]; then
    error "can not be run as root"
fi

# get the cannonical name directory name
mkdir -p "$directory"
if ! pushd "$directory" > /dev/null ; then 
    error "$directory is not accessible"
fi
directory=`pwd`
popd > /dev/null

if [[ "$directory" == "/" ]]; then
    error "directory $directory can not be root"
fi

parent=`dirname "$directory"`
grandparent=`dirname "$parent"`

if [[ "$parent" != "/tmp" && ( "$parent" == "/" || "$grandparent" == "/" ) ]]; then
    error "directory $directory can not be a subdirectory of $parent"
fi


# clean the directory if requested
rm -fR $rmopt $directory
mkdir -p "$directory"
