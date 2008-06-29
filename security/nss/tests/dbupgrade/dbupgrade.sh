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
