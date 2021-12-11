#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# libpkix_init_nist.sh
#

#
# Any test that uses NIST files should have a tag of either NIST-Test or
# NIST-Test-Files-Used at the command option so if there are no NIST files
# installed in the system, the test can be skipped
#

if [ -z "${NIST_FILES_DIR}" ] ; then
    Display ""
    Display "*******************************************************************************"
    Display "The environment variable NIST_FILES_DIR is not defined. Therefore"
    Display "tests depending on it will be skipped. To enable these tests set"
    Display "NIST_FILES_DIR to the directory where NIST Certificates and CRLs"
    Display "are located." 
    Display "*******************************************************************************"
    Display ""
    doNIST=0
else

    NIST=${NIST_FILES_DIR}
    doNIST=1
fi

#
# Any tests that use NIST Path Discovery files should have a tag of NIST-PDTest
# at the command option so if there are no NIST Path Discovery files
# installed in the system, the test can be skipped
#
if [ ${doPD} -eq 1 -a -z "${PDVAL}" ] ; then

    Display ""
    Display "*******************************************************************************"
    Display "The environment variable PDVAL is not defined. Therefore tests"
    Display "depending on it will be skipped. To enable these tests set PDVAL to"
    Display "the directory where NIST Path Discovery Certificates are located." 
    Display "*******************************************************************************"
    Display ""
    doNIST_PDTest=0
else

    NIST_PDTEST=${PDVAL}
    doNIST_PDTest=1
fi

#
# Any tests that use an OCSP Server should have a tag of OCSP-Test at the
# command option so if there is no OCSP Server installed in the system, the
# test can be skipped
#
if [  ${doOCSP} -eq 1 -a -z "${OCSP}" ] ; then

    Display ""
    Display "*******************************************************************************"
    Display "The environment variable OCSP is not defined. Therefore tests"
    Display "depending on it will be skipped. To enable these tests set OCSP"
    Display "non-NULL (the actual URI used is taken from the AIA extension)." 
    Display "*******************************************************************************"
    Display ""
    doOCSPTest=0
else
    doOCSPTest=1
fi
