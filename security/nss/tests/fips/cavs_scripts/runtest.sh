#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
TESTDIR=${1-.}
COMMAND=${2-run}
DEFAULT_TESTS="aes aesgcm dsa ecdsa hmac kas kbkdf tls ike rng rsa sha tdea"
shift;
shift;
TESTS=${@-$DEFAULT_TESTS}
result=0
for i in $TESTS
do
    echo "********************Running $i tests"
    sh ./${i}.sh ${TESTDIR} ${COMMAND}
    last_result=$?
    if [ $last_result != 0 ]; then
	echo "CAVS test $i had $last_result failure(s)"
    fi
    result=`expr $result + $last_result`
done
exit $result
