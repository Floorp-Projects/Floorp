#! /bin/sh
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
#   Sonja Mirtitsch Sun Microsystems
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

    
    Echo "test opening the database read/write in a nonexisting directory"
    ${BINDIR}/certutil -L -X -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "Certutil succeeded in a nonexisting directory $ret"
    else
      html_passed "Certutil didn't work in a nonexisting dir $ret" 
    fi
    ${BINDIR}/dbtest -r -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "Dbtest readonly succeeded in a nonexisting directory $ret"
    else
      html_passed "Dbtest readonly didn't work in a nonexisting dir $ret" 
    fi

    Echo "test force opening the database in a nonexisting directory"
    ${BINDIR}/dbtest -f -d ./non_existant_dir
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest force failed in a nonexisting directory $ret"
    else
      html_passed "Dbtest force succeeded in a nonexisting dir $ret"
    fi

    Echo "test opening the database readonly in an empty directory"
    mkdir $EMPTY_DIR
    ${BINDIR}/tstclnt -h  ${HOST}  -d $EMPTY_DIR 
    ret=$?
    if [ $ret -ne 1 ]; then
      html_failed "Tstclnt succeded in an empty directory $ret"
    else
      html_passed "Tstclnt didn't work in an empty dir $ret"
    fi
    ${BINDIR}/dbtest -r -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "Dbtest readonly succeeded in an empty directory $ret"
    else
      html_passed "Dbtest readonly didn't work in an empty dir $ret" 
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    ${BINDIR}/dbtest -i -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest logout after empty DB Init looses key $ret"
    else
      html_passed "Dbtest logout after empty DB Init has key" 
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    ${BINDIR}/dbtest -i -p pass -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest password DB Init looses needlogin state $ret"
    else
      html_passed "Dbtest password DB Init maintains needlogin state" 
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    ${BINDIR}/certutil -D -n xxxx -d $EMPTY_DIR #created DB
    ret=$?
    if [ $ret -ne 255 ]; then 
        html_failed "Certutil succeeded in deleting a cert in an empty directory $ret"
    else
        html_passed "Certutil didn't work in an empty dir $ret"
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    Echo "test force opening the database  readonly in a empty directory"
    ${BINDIR}/dbtest -r -f -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest force readonly failed in an empty directory $ret"
    else
      html_passed "Dbtest force readonly succeeded in an empty dir $ret"
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

    ${BINDIR}/dbtest -d $RONLY_DIR
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "Dbtest r/w succeeded in an readonly directory $ret"
    else
      html_passed "Dbtest r/w didn't work in an readonly dir $ret" 
    fi
    ${BINDIR}/certutil -D -n "TestUser" -d .
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "Certutil succeeded in deleting a cert in an readonly directory $ret"
    else
        html_passed "Certutil didn't work in an readonly dir $ret"
    fi
    
    Echo "test opening the database ronly in a readonly directory"

    ${BINDIR}/dbtest -d $RONLY_DIR -r
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest readonly failed in a readonly directory $ret"
    else
      html_passed "Dbtest readonly succeeded in a readonly dir $ret" 
    fi

    Echo "test force opening the database  r/w in a readonly directory"
    ${BINDIR}/dbtest -d $RONLY_DIR -f
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest force failed in a readonly directory $ret"
    else
      html_passed "Dbtest force succeeded in a readonly dir $ret"
    fi

    Echo "ls -l $RONLY_DIR"
    ls -ld $RONLY_DIR $RONLY_DIR/*

}

################## main #################################################

dbtest_init 
dbtest_main >$DBTEST_LOG 2>&1
dbtest_cleanup
