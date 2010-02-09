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
# The Original Code is the Network Security Services
#
# The Initial Developer of the Original Code is Sun Microsystems, Inc.
# Portions created by the Initial Developer are Copyright (C) 2007-2009
# Sun Microsystems, Inc. All Rights Reserved.
#
# Contributor(s):
#   Slavomir Katuscak <slavomir.katuscak@sun.com>, Sun Microsystems, Inc.
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

OS=`uname -s`
ARCH=`uname -p`
SCRIPT_DIR=`pwd`
DATE=`date +%Y%m%d`

if [ $# -ne 1 ]; then
    echo "Usage: $0 [securitytip|securityjes5]"
    exit 1
fi

BRANCH="$1"

if [ "${BRANCH}" != "securitytip" -a "${BRANCH}" != "securityjes5" ]; then
    echo "Usage: $0 [securitytip|securityjes5]"
    exit 1
fi

COV_DIR="/share/builds/mccrel3/security/coverage"
BRANCH_DIR="${COV_DIR}/${BRANCH}"
DATE_DIR="${BRANCH_DIR}/${DATE}-${ARCH}"
CVS_DIR="${DATE_DIR}/cvs_mozilla"
TCOV_DIR="${DATE_DIR}/tcov_mozilla"

CVS_CHECKOUT_BRANCH="cvs_checkout_${BRANCH}"

export HOST=`hostname`
export DOMSUF=red.iplanet.com

export NSS_ENABLE_ECC=1
export NSS_ECC_MORE_THAN_SUITE_B=1
export IOPR_HOSTADDR_LIST="dochinups.red.iplanet.com"
export NSS_AIA_PATH="/share/builds/mccrel3/security/aia_certs"
export NSS_AIA_HTTP="http://cindercone.red.iplanet.com/share/builds/mccrel3/security/aia_certs"

export USE_TCOV=1
export SUN_PROFDATA_DIR="${DATE_DIR}"
export SUN_PROFDATA="tcov_data"

if [ "${OS}" != "SunOS" ]; then
    echo "OS not supported"
    exit 1
fi

case "${ARCH}" in 
"sparc")
    export PATH="/usr/dist/share/sunstudio_sparc,v12.0/SUNWspro/prod/bin:/usr/sfw/bin:/usr/bin:/usr/ccs/bin:/usr/ucb:/tools/ns/bin:/usr/local/bin"
    ;;
"i386")
    export PATH="/usr/dist/share/sunstudio_i386,v12.0/SUNWspro/bin:/usr/sfw/bin:/usr/bin:/usr/ccs/bin:/usr/ucb:/tools/ns/bin:/usr/local/bin"
    ;;
*)
    echo "Platform not supported"
    exit 1
    ;;
esac

cvs_checkout_securitytip()
{
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/nsprpub
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/dbm
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/security/dbm
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/security/coreconf
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/security/nss
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A mozilla/security/jss
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_1_RTM mozilla/security/nss/lib/freebl/ecl/ecl-curve.h
}

cvs_checkout_securityjes5()
{
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSPR_4_6_BRANCH mozilla/nsprpub
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_BRANCH mozilla/dbm
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_BRANCH mozilla/security/dbm
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_BRANCH mozilla/security/coreconf
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_BRANCH mozilla/security/nss
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r JSS_4_2_BRANCH mozilla/security/jss
    cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co -A -r NSS_3_11_1_RTM mozilla/security/nss/lib/freebl/ecl/ecl-curve.h
}

cvs_checkout()
{
    rm -rf "${DATE_DIR}"
    mkdir -p "${CVS_DIR}"
    cd "${CVS_DIR}"

    ${CVS_CHECKOUT_BRANCH}
}

run_build()
{
    cd "${CVS_DIR}/mozilla/security/nss"
    gmake nss_build_all
}

run_tests()
{
    cd "${CVS_DIR}/mozilla/security/nss/tests"
    ./all.sh
}

process_results()
{
    rm -rf "${TCOV_DIR}"
    mkdir -p "${TCOV_DIR}"

    cat "${SUN_PROFDATA_DIR}/${SUN_PROFDATA}/tcovd" | grep SRCFILE | grep "${CVS_DIR}/.*.c$" | sed "s:[^/]*\(.*\):\1:" | sort -u |
    while read line
    do
	DIR=`echo "${line}" | sed "s:${CVS_DIR}/\(.*\)/.*:\1:"`
	FILE=`echo "${line}" | sed "s:.*/\(.*\):\1:"`

	mkdir -p "${TCOV_DIR}/${DIR}"
	tcov -o "${TCOV_DIR}/${DIR}/$FILE" -x "${SUN_PROFDATA}" $line >/dev/null 2>&1
    done
}

cvs_checkout
run_build
run_tests
process_results

cd "${SCRIPT_DIR}"
./report.sh "${BRANCH}" "${DATE}" "${ARCH}"  

exit 0

