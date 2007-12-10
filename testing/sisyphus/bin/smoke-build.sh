#!/bin/bash -e
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

if [[ -z "$1" ]]; then
    echo smoke-build.sh directorypattern
    exit 1
fi

for filepath in $@; do
    echo $filepath

    base=`basename $filepath`

    version=`echo $base|sed 's/[^0-9.]*\([0-9]*\).*/\1/'`
    product=`echo $base|sed 's/\([^-]*\)-.*/\1/'`

    case $version in
	1) branch=1.8.0;;
	2) branch=1.8.1;;
	3) branch=1.9.0;;
    esac

    echo $product $branch

    if ! install-build.sh  -p "$product" -b "$branch" -x "/tmp/$product-$branch" \
	-f "$filepath"; then
	error "installing build $product $branch into /tmp/$product-$branch"
    fi

    if [[ "$product" == "thunderbird" ]]; then
	template="-L ${TEST_DIR}/profiles/imap"
    else
	unset template
    fi

    if ! create-profile.sh -p "$product" -b "$branch" \
	-x "/tmp/$product-$branch" \
	-D "/tmp/$product-$branch-profile" -N "$product-$branch-profile" \
	-U ${TEST_DIR}/prefs/test-user.js \
	$template; then
	error "creating profile $product-$branch-profile at /tmp/$product-$branch"
    fi

    if ! install-extensions.sh -p "$product" -b "$branch" \
	-x "/tmp/$product-$branch" \
	-N "$product-$branch-profile" \
	-E ${TEST_DIR}/xpi; then
        error "installing extensions from ${TEST_DIR}/xpi"
    fi

    if ! check-spider.sh -p "$product" -b "$branch" \
	-x "/tmp/$product-$branch" \
	-N "$product-$branch-profile"; then
        error "check-spider.sh failed."
    fi

    uninstall-build.sh  -p "$product" -b "$branch" -x "/tmp/$product-$branch"

done
