#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/policy/policy.sh
#
# Script to test NSS crypto policy code
#
########################################################################

ignore_blank_lines()
{
  LC_ALL=C grep -v '^[[:space:]]*\(#\|$\)' "$1"
}

policy_run_tests()
{
  html_head "CRYPTO-POLICY"

  POLICY_INPUT=${QADIR}/policy/crypto-policy.txt

  ignore_blank_lines ${POLICY_INPUT} | \
  while read value policy match testname
  do
    echo "$SCRIPTNAME: running \"$testname\" ----------------------------"
    policy=`echo ${policy} | sed -e 's;_; ;g'`
    match=`echo ${match} | sed -e 's;_; ;g'`
    POLICY_FILE="${TMP}/nss-policy"

    echo "$SCRIPTNAME: policy: \"$policy\""

    cat > "$POLICY_FILE" << ++EOF++
library=
name=Policy
NSS=flags=policyOnly,moduleDB
++EOF++
    echo "config=\"${policy}\"" >> "$POLICY_FILE"
    echo "" >> "$POLICY_FILE"

    nss-policy-check "$POLICY_FILE" >${TMP}/$HOST.tmp.$$ 2>&1
    ret=$?
    cat ${TMP}/$HOST.tmp.$$

    html_msg $ret $value "\"${testname}\"" \
        "produced a returncode of $ret, expected is $value"

    egrep "${match}" ${TMP}/$HOST.tmp.$$
    ret=$?
    html_msg $ret 0 "\"${testname}\" output is expected to match \"${match}\""

  done
}

policy_run_tests
