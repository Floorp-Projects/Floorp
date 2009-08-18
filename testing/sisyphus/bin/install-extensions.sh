#!/bin/bash -e
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
# Portions created by the Initial Developer are Copyright (C) 2005.
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

source $TEST_DIR/bin/library.sh

TEST_STARTUP_TRIES=${TEST_STARTUP_TRIES:-3}

#
# options processing
#
options="p:b:x:N:E:d:"
function usage()
{
    cat <<EOF
usage:
$SCRIPT -p product -b branch -x executablepath -N profilename -E extensiondir
       [-d datafiles]

variable            description
===============     ============================================================
-p product          required. firefox, thunderbird or fennec
-b branch           required. one of 1.8.0 1.8.1 1.9.0 1.9.1 1.9.2
-x executablepath   required. directory-tree containing executable named
                    'product'
-N profilename      required. profile name
-E extensiondir       required. path to directory containing xpis to be installed
-d datafiles        optional. one or more filenames of files containing
            environment variable definitions to be included.

note that the environment variables should have the same names as in the
"variable" column.

EOF
    exit 1
}

unset product branch executablepath profilename extensiondir datafiles

while getopts $options optname ;
do
    case $optname in
        p) product=$OPTARG;;
        b) branch=$OPTARG;;
        x) executablepath=$OPTARG;;
        N) profilename=$OPTARG;;
        E) extensiondir=$OPTARG;;
        d) datafiles=$OPTARG;;
    esac
done

# include environment variables
loaddata $datafiles

if [[ -z "$product" || -z "$branch" || \
    -z "$executablepath" || -z "$profilename" || -z "$extensiondir" ]]; then
    usage
fi

checkProductBranch $product $branch

if echo $profilename | egrep -qiv '[a-z0-9_]'; then
    error "profile name must consist of letters, digits or _" $LINENO
fi

executable=`get_executable $product $branch $executablepath`
executableextensiondir=`dirname $executable`/extensions

# create directory to contain installed extensions
if [[ ! -d /tmp/sisyphus/extensions ]]; then
    create-directory.sh -n -d /tmp/sisyphus/extensions
fi

for extensionloc in $extensiondir/all/*.xpi $extensiondir/$OSID/*.xpi; do
    if [[ $extensionloc == "$extensiondir/all/*.xpi" ]]; then
        continue
    fi
    if [[ $extensionloc == "$extensiondir/$OSID/*.xpi" ]]; then
        continue
    fi

    extensionname=`xbasename $extensionloc .xpi`
    extensioninstalldir=/tmp/sisyphus/extensions/$extensionname

    if [[ "$OSID" == "nt" ]]; then
        extensionosinstalldir=`cygpath -a -w $extensioninstalldir`
    else
        extensionosinstalldir=$extensioninstalldir
    fi
    echo installing $extensionloc

    # unzip the extension if it does not already exist.
    if [[ ! -e $extensioninstalldir ]]; then
        create-directory.sh -n -d $extensioninstalldir
        unzip -d $extensioninstalldir $extensionloc
    fi

    extensionuuid=`perl $TEST_DIR/bin/get-extension-uuid.pl $extensioninstalldir/install.rdf`
    if [[ ! -e $executableextensiondir/$extensionuuid ]]; then
        echo $extensionosinstalldir > $executableextensiondir/$extensionuuid
    fi

done

# restart to make extension manager happy
if ! $TEST_DIR/bin/timed_run.py ${TEST_STARTUP_TIMEOUT} "install extensions - first restart" \
    $executable -P $profilename "http://${TEST_HTTP}/bin/install-extensions-1.html"; then
    echo "Ignoring 1st failure to load the install-extensions page"
fi

exit 0
