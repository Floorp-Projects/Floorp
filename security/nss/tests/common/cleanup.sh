#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


if [ -z "${CLEANUP}" -o "${CLEANUP}" = "${SCRIPTNAME}" ]; then
    echo
    echo "SUMMARY:"
    echo "========"
    echo "NSS variables:"
    echo "--------------"
    echo "HOST=${HOST}"
    echo "DOMSUF=${DOMSUF}"
    echo "BUILD_OPT=${BUILD_OPT}"
    if [ "${OS_ARCH}" = "Linux" ]; then
        echo "USE_X32=${USE_X32}"
    fi
    echo "USE_64=${USE_64}"
    echo "NSS_CYCLES=\"${NSS_CYCLES}\""
    echo "NSS_TESTS=\"${NSS_TESTS}\""
    echo "NSS_SSL_TESTS=\"${NSS_SSL_TESTS}\""
    echo "NSS_SSL_RUN=\"${NSS_SSL_RUN}\""
    echo "NSS_AIA_PATH=${NSS_AIA_PATH}"
    echo "NSS_AIA_HTTP=${NSS_AIA_HTTP}"
    echo "NSS_AIA_OCSP=${NSS_AIA_OCSP}"
    echo "IOPR_HOSTADDR_LIST=${IOPR_HOSTADDR_LIST}"
    echo "PKITS_DATA=${PKITS_DATA}"
    echo
    echo "Tests summary:"
    echo "--------------"
    LINES_CNT=$(cat ${RESULTS} | grep ">Passed<" | wc -l | sed s/\ *//)
    echo "Passed:             ${LINES_CNT}"
    LINES_CNT=$(cat ${RESULTS} | grep ">Failed<" | wc -l | sed s/\ *//)
    echo "Failed:             ${LINES_CNT}"
    LINES_CNT=$(cat ${RESULTS} | grep ">Failed Core<" | wc -l | sed s/\ *//)
    echo "Failed with core:   ${LINES_CNT}"
    LINES_CNT=$(cat ${RESULTS} | grep ">Unknown<" | wc -l | sed s/\ *//)
    echo "Unknown status:     ${LINES_CNT}"
    if [ ${LINES_CNT} -gt 0 ]; then
        echo "TinderboxPrint:Unknown: ${LINES_CNT}"
    fi
    echo

    html "END_OF_TEST<BR>"
    html "</BODY></HTML>" 
    rm -f ${TEMPFILES} 2>/dev/null
fi
