#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/dbupgrade/dbupgrade.sh
#
# Script to upgrade databases to Shared DB
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################ dbupgrade_init ############################
# local shell function to initialize this script
########################################################################
dbupgrade_init()
{
	if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then 
		cd ${QADIR}/common
		. ./init.sh
	fi
	
	if [ ! -r "${CERT_LOG_FILE}" ]; then  # we need certificates here
		cd ${QADIR}/cert 
		. ./cert.sh
	fi

	if [ ! -d ${HOSTDIR}/SDR ]; then  # we also need sdr as well
		cd ${QADIR}/sdr
		. ./sdr.sh
	fi
	
	SCRIPTNAME=dbupgrade.sh
	if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
		CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
	fi
	
	echo "$SCRIPTNAME: DB upgrade tests ==============================="
}

############################ dbupgrade_main ############################
# local shell function to upgrade certificate databases
########################################################################
dbupgrade_main()
{
	# 'reset' the databases to initial values
	echo "Reset databases to their initial values:"
	cd ${HOSTDIR}
	${BINDIR}/certutil -D -n objsigner -d alicedir 2>&1
	${BINDIR}/certutil -M -n FIPS_PUB_140_Test_Certificate -t "C,C,C" -d fips -f ${FIPSPWFILE} 2>&1
	${BINDIR}/certutil -L -d fips 2>&1
	rm -f smime/alicehello.env
	
	# test upgrade to the new database
	echo "nss" > ${PWFILE}
	html_head "Legacy to shared Library update"
	dirs="alicedir bobdir CA cert_extensions client clientCA dave eccurves eve ext_client ext_server SDR server serverCA tools/copydir"
	for i in $dirs
	do
		echo $i
		if [ -d $i ]; then
			echo "upgrading db $i"
			${BINDIR}/certutil -G -g 512 -d sql:$i -f ${PWFILE} -z ${NOISE_FILE} 2>&1
			html_msg $? 0 "Upgrading $i"
		else
			echo "skipping db $i"
			html_msg 0 0 "No directory $i"
		fi
	done
	
	if [ -d fips ]; then
		echo "upgrading db fips"
		${BINDIR}/certutil -S -g 512 -n tmprsa -t "u,u,u" -s "CN=tmprsa, C=US" -x -d sql:fips -f ${FIPSPWFILE} -z ${NOISE_FILE} 2>&1
		html_msg $? 0 "Upgrading fips"
		# remove our temp certificate we created in the fist token
		${BINDIR}/certutil -F -n tmprsa -d sql:fips -f ${FIPSPWFILE} 2>&1
		${BINDIR}/certutil -L -d sql:fips 2>&1
	fi
	
	html "</TABLE><BR>"
}

########################## dbupgrade_cleanup ###########################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
dbupgrade_cleanup()
{
	cd ${QADIR}
	. common/cleanup.sh
}

################################# main #################################

dbupgrade_init
dbupgrade_main
dbupgrade_cleanup
