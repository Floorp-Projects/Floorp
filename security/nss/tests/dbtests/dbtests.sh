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
  SCRIPTNAME="dbtests.sh"
  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" ] ; then
      cd ../common
      . ./init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi

  SCRIPTNAME="dbtests.sh"
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
  chmod a+rw $RONLY_DIR
  . common/cleanup.sh
}

Echo()
{
    echo
    echo "---------------------------------------------------------------"
    echo "| $*"
    echo "---------------------------------------------------------------"
}
dbtest_main()
{
    cd ${HOSTDIR}

    
    Echo "test opening the database readonly in a nonexisting directory"
    certutil -L -X -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "<TR><TD> Certutil succeeded in a nonexisting directory $ret"
    else
      html_passed "<TR><TD> Certutil failed in a nonexisting dir $ret" 
    fi
    dbtest -r -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "<TR><TD> Dbtest readonly succeeded in a nonexisting directory $ret"
    else
      html_passed "<TR><TD> Dbtest readonly failed in a nonexisting dir $ret" 
    fi

    Echo "test force opening the database in a nonexisting directory"
    dbtest -f -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "<TR><TD> Dbtest force failed in a nonexisting directory $ret"
    else
      html_passed "<TR><TD> Dbtest force succeeded in a nonexisting dir $ret"
    fi

    Echo "test opening the database readonly in an empty directory"
    mkdir $EMPTY_DIR
    tstclnt -h  ${HOST}  -d $EMPTY_DIR 
    ret=$?
    if [ $ret -ne 1 ]; then
      html_failed "<TR><TD> Tstclnt succeded in an empty directory $ret"
    else
      html_passed "<TR><TD> Tstclnt failed in an empty dir $ret"
    fi
    dbtest -r -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "<TR><TD> Dbtest readonly succeeded in an empty directory $ret"
    else
      html_passed "<TR><TD> Dbtest readonly failed in an empty dir $ret" 
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    certutil -D -n xxxx -d $EMPTY_DIR #created DB
    ret=$?
    if [ $ret -ne 255 ]; then 
        html_failed "<TR><TD> Certutil succeeded in deleting a cert in an empty directory $ret"
    else
        html_passed "<TR><TD> Certutil failed in an empty dir $ret"
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    Echo "test force opening the database  readonly in a empty directory"
    dbtest -r -f -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "<TR><TD> Dbtest force readonly failed in an empty directory $ret"
    else
      html_passed "<TR><TD> Dbtest force readonly succeeded in an empty dir $ret"
    fi

    Echo "test opening the database r/w in a readonly directory"
    mkdir $RONLY_DIR
    cp -r ${CLIENTDIR}/* $RONLY_DIR
    chmod -w $RONLY_DIR $RONLY_DIR/*

    # On Mac OS X 10.1, if we do a "chmod -w" on files in an
    # NFS-mounted directory, it takes several seconds for the
    # first open to see the files are readonly, but subsequent
    # opens immediately see the files are readonly.  As a
    # workaround we open the files once first.  (Bug 185074)
    if [ "${OS_ARCH}" = "Darwin" ]; then
        cat $RONLY_DIR/* > /dev/null
    fi

    dbtest -d $RONLY_DIR
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "<TR><TD> Dbtest r/w succeeded in an readonly directory $ret"
    else
      html_passed "<TR><TD> Dbtest r/w failed in an readonly dir $ret" 
    fi
    certutil -D -n "TestUser" -d .
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "<TR><TD> Certutil succeeded in deleting a cert in an readonly directory $ret"
    else
        html_passed "<TR><TD> Certutil failed in an readonly dir $ret"
    fi
    
    Echo "test opening the database ronly in a readonly directory"

    dbtest -d $RONLY_DIR -r
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "<TR><TD> Dbtest readonly failed in a readonly directory $ret"
    else
      html_passed "<TR><TD> Dbtest readonly succeeded in a readonly dir $ret" 
    fi

    Echo "test force opening the database  r/w in a readonly directory"
    dbtest -d $RONLY_DIR -f
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "<TR><TD> Dbtest force failed in a readonly directory $ret"
    else
      html_passed "<TR><TD> Dbtest force succeeded in a readonly dir $ret"
    fi

    Echo "ls -l $RONLY_DIR"
    ls -ld $RONLY_DIR $RONLY_DIR/*

}

################## main #################################################

dbtest_init 
dbtest_main >$DBTEST_LOG 2>&1
dbtest_cleanup
