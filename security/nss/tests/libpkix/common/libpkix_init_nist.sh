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
