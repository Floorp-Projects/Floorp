#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
  RONLY_DIR=${HOSTDIR}/ronlydir
  EMPTY_DIR=${HOSTDIR}/emptydir
  CONFLICT_DIR=${HOSTDIR}/conflictdir
  THREAD_DIR=${HOSTDIR}/threadir

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
    ${BINDIR}/certutil -L -X -d ./non_existent_dir
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "Certutil succeeded in a nonexisting directory $ret"
    else
      html_passed "Certutil didn't work in a nonexisting dir $ret"
    fi
    ${BINDIR}/dbtest -r -d ./non_existent_dir
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "Dbtest readonly succeeded in a nonexisting directory $ret"
    else
      html_passed "Dbtest readonly didn't work in a nonexisting dir $ret"
    fi

    Echo "test force opening the database in a nonexisting directory"
    ${BINDIR}/dbtest -f -d ./non_existent_dir
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
      html_failed "Dbtest logout after empty DB Init loses key $ret"
    else
      html_passed "Dbtest logout after empty DB Init has key"
    fi
    rm -rf $EMPTY_DIR/* 2>/dev/null
    ${BINDIR}/dbtest -i -p pass -d $EMPTY_DIR
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Dbtest password DB Init loses needlogin state $ret"
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

    # skipping the next two tests when user is root,
    # otherwise they would fail due to rooty powers
    if [ $UID -ne 0 ]; then
      ${BINDIR}/dbtest -d $RONLY_DIR
    ret=$?
    if [ $ret -ne 46 ]; then
      html_failed "Dbtest r/w succeeded in a readonly directory $ret"
    else
      html_passed "Dbtest r/w didn't work in an readonly dir $ret"
    fi
    else
      html_passed "Skipping Dbtest r/w in a readonly dir because user is root"
    fi
    if [ $UID -ne 0 ]; then
      ${BINDIR}/certutil -D -n "TestUser" -d .
    ret=$?
    if [ $ret -ne 255 ]; then
      html_failed "Certutil succeeded in deleting a cert in a readonly directory $ret"
    else
      html_passed "Certutil didn't work in an readonly dir $ret"
    fi
    else
        html_passed "Skipping Certutil delete cert in a readonly directory test because user is root"
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

    mkdir ${CONFLICT_DIR}
    Echo "test creating a new cert with a conflicting nickname"
    cd ${CONFLICT_DIR}
    pwd
    ${BINDIR}/certutil -N -d ${CONFLICT_DIR} -f ${R_PWFILE}
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Nicknane conflict test failed, couldn't create database $ret"
    else
      ${BINDIR}/certutil -A -n alice -t ,, -i ${R_ALICEDIR}/Alice.cert -d ${CONFLICT_DIR}
      ret=$?
      if [ $ret -ne 0 ]; then
        html_failed "Nicknane conflict test failed, couldn't import alice cert $ret"
      else
        ${BINDIR}/certutil -A -n alice -t ,, -i ${R_BOBDIR}/Bob.cert -d ${CONFLICT_DIR}
        ret=$?
        if [ $ret -eq 0 ]; then
          html_failed "Nicknane conflict test failed, could import conflict nickname $ret"
        else
          html_passed "Nicknane conflict test, could not import conflict nickname $ret"
        fi
      fi
    fi

    Echo "test importing an old cert to a conflicting nickname"
    # first, import the certificate
    ${BINDIR}/certutil -A -n bob -t ,, -i ${R_BOBDIR}/Bob.cert -d ${CONFLICT_DIR}
    # now import with a different nickname
    ${BINDIR}/certutil -A -n alice -t ,, -i ${R_BOBDIR}/Bob.cert -d ${CONFLICT_DIR}
    # the old one should still be there...
    ${BINDIR}/certutil -L -n bob -d ${CONFLICT_DIR}
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Nicknane conflict test-setting nickname conflict incorrectly worked"
    else
      html_passed "Nicknane conflict test-setting nickname conflict was correctly rejected"
    fi
    # import a token private key and make sure the corresponding public key is
    # created
    ${BINDIR}/pk11importtest -d ${CONFLICT_DIR} -f ${R_PWFILE}
    ret=$?
    if [ $ret -ne 0 ]; then
      html_failed "Importing Token Private Key does not create the corrresponding Public Key"
    else
      html_passed "Importing Token Private Key correctly creates the corrresponding Public Key"
    fi


    if [ "${NSS_DEFAULT_DB_TYPE}" = "sql" ] ; then
      LOOPS=${NSS_SDB_THREAD_LOOPS-7}
      THREADS=${NSS_SDB_THREAD_THREADS-30}
      mkdir -p ${THREAD_DIR}
      Echo "testing for thread starvation while creating keys"
      ${BINDIR}/certutil -N -d ${THREAD_DIR} --empty-password
      ${BINDIR}/sdbthreadtst -l ${LOOPS} -t ${THREADS} -d ${THREAD_DIR}
      ret=$?
      case "$ret" in
      "0")
         html_passed "Successfully completed ${LOOPS} loops in ${THREADS} threads without failure."
         ;;
      "2")
         html_failed "sdbthreadtst failed for some environment reason (like lack of memory)"
         ;;
      "1")
         html_failed "sdbthreadtst failed do to starvation using ${LOOPS} loops and ${THREADS} threads."
         ;;
      *)
         html_failed "sdbthreadtst failed with an unrecognized error code."
      esac
    fi
}

################## main #################################################

dbtest_init
dbtest_main 2>&1
dbtest_cleanup
