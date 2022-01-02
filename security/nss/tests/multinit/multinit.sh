#! /bin/sh  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/multinit/multinit.sh
#
# Script to test NSS multinit
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## multinit_init ##############################
# local shell function to initialize this script
########################################################################
multinit_init()
{
  SCRIPTNAME=multinit.sh      # sourced - $0 would point to all.sh

  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi

  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
      cd ../common
      . ./init.sh
  fi
  if [ ! -r $CERT_LOG_FILE ]; then  # we need certificates here
      cd ../cert
      . ./cert.sh
  fi
  SCRIPTNAME=multinit.sh

  html_head "MULTI Tests"

  grep "SUCCESS: SMIME passed" $CERT_LOG_FILE >/dev/null || {
      Exit 11 "Fatal - S/MIME of cert.sh needs to pass first"
  }

  # set up our directories
  MULTINITDIR=${HOSTDIR}/multinit
  MULTINITDIR_1=${MULTINITDIR}/dir1
  MULTINITDIR_2=${MULTINITDIR}/dir2
  MULTINITDIR_3=${MULTINITDIR}/dir3
  R_MULINITDIR=../multinit
  R_MULTINITDIR_1=${R_MULTINITDIR}/dir1
  R_MULTINITDIR_2=${R_MULTINITDIR}/dir2
  R_MULTINITDIR_3=${R_MULTINITDIR}/dir3
  # first create them all
  mkdir -p ${MULTINITDIR}
  mkdir -p ${MULTINITDIR_1}
  mkdir -p ${MULTINITDIR_2}
  mkdir -p ${MULTINITDIR_3}
  # now copy them fro alice, bob, and dave
  cd ${MULTINITDIR}
  cp ${P_R_ALICEDIR}/* ${MULTINITDIR_1}/
  cp ${P_R_BOBDIR}/*   ${MULTINITDIR_2}/
  cp ${P_R_DAVEDIR}/*  ${MULTINITDIR_3}/
  # finally delete the RootCerts module to keep the certificate noice in the
  # summary lines down
  echo | modutil -delete RootCerts -dbdir ${MULTINITDIR_1}
  echo | modutil -delete RootCerts -dbdir ${MULTINITDIR_2}
  echo | modutil -delete RootCerts -dbdir ${MULTINITDIR_3}
  MULTINIT_TESTS=${QADIR}/multinit/multinit.txt
}


############################## multinit_main ##############################
# local shell function to test basic signed and enveloped messages 
# from 1 --> 2"
########################################################################
multinit_main()
{
  html_head "Multi init interface testing"
  exec < ${MULTINIT_TESTS}
  while read order commands shutdown_type dirs readonly testname
  do
    if [ "$order" != "#" ]; then
	read tag expected_result

	# handle the case where we expect different results based on
	# the database type.
	if [ "$tag" != "all" ]; then
	    read tag2 expected_result2
	    if [ "$NSS_DEFAULT_DB_TYPE" == "$tag2" ]; then
		expected_result=$expected_result2
	    fi
	fi

	# convert shutdown type to option flags
	shutdown_command="";
	if [ "$shutdown_type" == "old" ]; then
	   shutdown_command="--oldStype"
	fi

	# convert read only to option flags
	ro_command="";
	case $readonly in
        all)  ro_command="--main_readonly --lib1_readonly --lib2_readonly";;
        libs) ro_command="--lib1_readonly --lib2_readonly";;
        main) ro_command="--main_readonly";;
        lib1) ro_command="--lib1_readonly";;
        lib2) ro_command="--lib2_readonly";;
	none) ;;
	*) ;;
	esac

	# convert commands to option flags
	main_command=`echo $commands | sed -e 's;,.*$;;'`
	lib1_command=`echo $commands | sed -e 's;,.*,;+&+;' -e 's;^.*+,;;' -e 's;,+.*$;;'`
	lib2_command=`echo $commands | sed -e 's;^.*,;;'`

	# convert db's to option flags
	main_db=`echo $dirs | sed -e 's;,.*$;;'`
	lib1_db=`echo $dirs | sed -e 's;,.*,;+&+;' -e 's;^.*+,;;' -e 's;,+.*$;;'`
	lib2_db=`echo $dirs | sed -e 's;^.*,;;'`

	# show us the command we are executing
	echo ${PROFILETOOL} ${BINDIR}/multinit --order $order --main_command $main_command --lib1_command $lib1_command --lib2_command $lib2_command $shutdown_command --main_db $main_db --lib1_db $lib1_db --lib2_db $lib2_db $ro_command --main_token_name "Main" --lib1_token_name "Lib1" --lib2_token_name "Lib2" --verbose --summary 

	# execute the command an collect the result. Most of the user
	# visible output goes to stderr, so it's not captured by the pipe
	actual_result=`${PROFILETOOL} ${BINDIR}/multinit --order $order --main_command $main_command --lib1_command $lib1_command --lib2_command $lib2_command $shutdown_command --main_db $main_db --lib1_db $lib1_db --lib2_db $lib2_db $ro_command --main_token_name "Main" --lib1_token_name "Lib1" --lib2_token_name "Lib2" --verbose --summary | grep "^result=" | sed -e 's;^result=;;'` 

	# show what we got and what we expected for diagnostic purposes
	echo "actual   = |$actual_result|"
	echo "expected = |$expected_result|"
	test  "$actual_result" == "$expected_result" 
	html_msg $? 0 "$testname"
    fi
  done
}
  
############################## multinit_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
multinit_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

multinit_init
multinit_main
multinit_cleanup
