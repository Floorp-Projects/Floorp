#!/bin/bash
#
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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Slavomir Katuscak <slavomir.katuscak@sun.com>, Sun Microsystems
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


if [ -z "${CLEANUP}" -o "${CLEANUP}" = "${SCRIPTNAME}" ]; then
    echo
    echo "SUMMARY:"
    echo "========"
    echo "NSS variables:"
    echo "--------------"
    echo "HOST=${HOST}"
    echo "DOMSUF=${DOMSUF}"
    echo "BUILD_OPT=${BUILD_OPT}"
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
