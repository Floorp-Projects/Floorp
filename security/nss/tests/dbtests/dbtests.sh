#! /bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# Sonja Mirtitsch Sun Microsystems
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

########################################################################
#
# mozilla/security/nss/tests/dbtest/dbtest.sh
#
# Certificate generating and handeling for NSS QA, can be included 
# multiple times from all.sh and the individual scripts
#
# needs to work on all Unix and Windows platforms
#
# included from (don't expect this to be up to date)
# --------------------------------------------------
#   all.sh
#   ssl.sh
#   smime.sh
#   tools.sh
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# FIXME - Netscape - NSS
########################################################################

############################## dbtest_init ###############################
# local shell function to initialize this script
########################################################################
dbtest_init()
{
  SCRIPTNAME="dbtest.sh"
  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" ] ; then
      cd ../common
      . init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . cert.sh
  fi

  SCRIPTNAME="dbtest.sh"
  DBTEST_LOG=${HOSTDIR}/dbtest.log    #we don't want all the errormessages 
         # in the output.log, otherwise we can't tell what's a real error
  RONLY_DIR=${HOSTDIR}/ronlydir
  EMPTY_DIR=${HOSTDIR}/emptydir
  grep "SUCCESS: SSL passed" $CERT_LOG_FILE >/dev/null || {
      html_head "SSL Test failure"
      Exit : "Fatal - SSL of cert.sh needs to pass first"
  }

  html_head "CERT and Key DB Tests"

}

############################## dbtest_cleanup ############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
dbtest_cleanup()
{
  html "</TABLE><BR>" 
  cd ${QADIR}
  . common/cleanup.sh
}

dbtest_main()
{
    cd ${HOSTDIR}
    certutil -L -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "<TR><TD> Certutil succeeded in a nonexisting directory $ret"
    else
      html_passed "<TR><TD> Certutil failed in a nonexisting dir $ret" 
    fi

    mkdir $EMPTY_DIR
    tstclnt -h  ${HOST}  -d $EMPTY_DIR 
    ret=$?
    if [ $ret -ne 1 ]; then
      html_failed "<TR><TD> tstclnt succeded in an empty directory $ret"
    else
      html_passed "<TR><TD> tstclnt failed in an empty dir $ret"
    fi
    certutil -D -n xxxx -d $EMPTY_DIR #created DB
    ret=$?
    if [ $ret -ne 255 ]; then 
        html_failed "<TR><TD> Certutil succeeded in deleting a cert in an empty directory $ret"
    else
        html_passed "<TR><TD> Certutil failed in an empty dir $ret"
    fi
    mkdir $RONLY_DIR
    cd $RONLY_DIR
    cp -r ${CLIENTDIR}/* .
    chmod -w * .
    certutil -D -n "TestUser" -d .
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "<TR><TD> Certutil succeeded in deleting a cert in an readonly directory $ret"
    else
        html_passed "<TR><TD> Certutil failed in an readonly dir $ret"
    fi
}

################## main #################################################

dbtest_init 
dbtest_main >$DBTEST_LOG 2>&1
dbtest_cleanup
