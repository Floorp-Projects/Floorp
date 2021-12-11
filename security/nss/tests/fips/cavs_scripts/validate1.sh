#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Validate1.sh is a helper shell script that each of the base test shell
# scripts call to help validate that the generated response (response)
# matches the known answer response (fax). Sometimes (depending on the
# individual tests) there are extraneous output in either or both response
# and fax files. These allow the caller to pass in additional sed commands
# to clear out those extraneous outputs before we compare the two files.
# The sed line always clears out Windows line endings, replaces tabs with
# spaces, and removed comments.
#
TESTDIR=${1-.}
request=${2}
extraneous_response=${3}
extraneous_fax=${4}
name=`basename $request .req`
echo ">>>>>  $name"
sed -e 's;;;g' -e 's;	; ;g' -e '/^#/d' $extraneous_response ${TESTDIR}/resp/${name}.rsp > /tmp/y1
# if we didn't generate any output, flag that as an error
size=`sum /tmp/y1 | awk '{ print $NF }'`
if [ $size -eq 0 ]; then
   echo "${TESTDIR}/resp/${name}.rsp: empty"
   exit 1;
fi
sed -e 's;;;g' -e 's;	; ;g' -e '/^#/d' $extraneous_fax ${TESTDIR}/fax/${name}.fax > /tmp/y2
diff -i -w -B /tmp/y1 /tmp/y2
