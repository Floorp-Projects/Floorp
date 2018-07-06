#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
TESTDIR=${1-.}
COMMAND=${2-run}
TESTS="aes aesgcm dsa ecdsa hmac kas tls rng rsa sha tdea"
for i in $TESTS
do
    echo "********************Running $i tests"
    sh ./${i}.sh ${TESTDIR} ${COMMAND}
done
