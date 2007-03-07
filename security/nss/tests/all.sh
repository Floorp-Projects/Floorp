#!/bin/sh
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
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

########################################################################
#
# mozilla/security/nss/tests/all.sh
#
# Script to start all available NSS QA suites on one machine
# this script is called or sourced by nssqa which runs on all required 
# platforms
#
# needs to work on all Unix and Windows platforms
#
# currently available NSS QA suites:
# --------------------------------------------------
#   cert.sh   - exercises certutil and creates certs necessary for all 
#               other tests
#   ssl.sh    - tests SSL V2 SSL V3 and TLS
#   smime.sh  - S/MIME testing
#   crmf.sh   - CRMF/CMMF testing
#   sdr.sh    - test NSS SDR
#   cipher.sh - test NSS ciphers
#   perf.sh   - Nightly performance measurments
#   tools.sh  - Tests the majority of the NSS tools
#   fips.sh   - Tests basic functionallity of NSS in FIPS-compliant mode
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# NOTE:
# -----
#    Unlike the old QA this is based on files sourcing each other
#    This is done to save time, since a great portion of time is lost
#    in calling and sourcing the same things multiple times over the
#    network. Also, this way all scripts have all shell function  available
#    and a completely common environment
#
# file tells the test suite that the output is going to a log, so any
#  forked() children need to redirect their output to prevent them from
#  being over written.
#
########################################################################

tests="cipher perf cert dbtests tools fips sdr crmf smime ssl ocsp"
TESTS=${TESTS:-$tests}
SCRIPTNAME=all.sh
CLEANUP="${SCRIPTNAME}"
cd `dirname $0`	# will cause problems if sourced 

#all.sh should be the first one to try to source the init 
if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
    cd common
    . ./init.sh
fi

for i in ${TESTS}
do
    SCRIPTNAME=${i}.sh
    if [ "$O_CRON" = "ON" ]
    then
        echo "Running tests for $i" >> ${LOGFILE}
        echo "TIMESTAMP $i BEGIN: `date`" >> ${LOGFILE}
        (cd ${QADIR}/$i ; . ./$SCRIPTNAME all file >> ${LOGFILE} 2>&1)
        echo "TIMESTAMP $i END: `date`" >> ${LOGFILE}
    else
        echo "Running tests for $i" | tee -a ${LOGFILE}
        echo "TIMESTAMP $i BEGIN: `date`" | tee -a ${LOGFILE}
        (cd ${QADIR}/$i ; . ./$SCRIPTNAME all file 2>&1 | tee -a ${LOGFILE})
        echo "TIMESTAMP $i END: `date`" | tee -a ${LOGFILE}
    fi
done

SCRIPTNAME=all.sh

. ${QADIR}/common/cleanup.sh
