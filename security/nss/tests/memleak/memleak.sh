#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/memleak/memleak.sh
#
# Script to test memory leaks in NSS
#
# needs to work on Solaris and Linux platforms, on others just print a message
# that OS is not supported
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################# memleak_init #############################
# local shell function to initialize this script 
########################################################################
memleak_init()
{
	if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
		cd ../common
		. ./init.sh
	fi
	
	if [ ! -r ${CERT_LOG_FILE} ]; then
		cd ${QADIR}/cert
		. ./cert.sh
	fi

	SCRIPTNAME="memleak.sh"
	if [ -z "${CLEANUP}" ] ; then
		CLEANUP="${SCRIPTNAME}"
	fi

	OLD_LIBRARY_PATH=${LD_LIBRARY_PATH}
	TMP_LIBDIR="${HOSTDIR}/tmp"
	TMP_STACKS="${HOSTDIR}/stacks"
	TMP_SORTED="${HOSTDIR}/sorted"
	TMP_COUNT="${HOSTDIR}/count"
	DBXOUT="${HOSTDIR}/dbxout"
	DBXERR="${HOSTDIR}/dbxerr"
	DBXCMD="${HOSTDIR}/dbxcmd"
	
	PORT=${PORT:-8443}
	
	MODE_LIST="NORMAL BYPASS FIPS"
	
	SERVER_DB="${HOSTDIR}/server_memleak"
	CLIENT_DB="${HOSTDIR}/client_memleak"
	cp -r ${HOSTDIR}/server ${SERVER_DB}
	cp -r ${HOSTDIR}/client ${CLIENT_DB}
	
	LOGDIR="${HOSTDIR}/memleak_logs"
	mkdir -p ${LOGDIR}

	FOUNDLEAKS="${LOGDIR}/foundleaks"
	
	REQUEST_FILE="${QADIR}/memleak/sslreq.dat"
	IGNORED_STACKS="${QADIR}/memleak/ignored"
	
	gline=`echo ${OBJDIR} | grep "_64_"`
	if [ -n "${gline}" ] ; then
		BIT_NAME="64"
	else
		BIT_NAME="32"
	fi
		
	case "${OS_NAME}" in
	"SunOS")
		DBX=`which dbx`
		AWK=nawk
		
		if [ $? -eq 0 ] ; then
			echo "${SCRIPTNAME}: DBX found: ${DBX}"
		else
			echo "${SCRIPTNAME}: DBX not found, skipping memory leak checking."
			exit 0
		fi
		
		PROC_ARCH=`uname -p`
				
		if [ "${PROC_ARCH}" = "sparc" ] ; then
			if [ "${BIT_NAME}" = "64" ] ; then
				FREEBL_DEFAULT="libfreebl_64fpu_3"
				FREEBL_LIST="${FREEBL_DEFAULT} libfreebl_64int_3"
			else
				FREEBL_DEFAULT="libfreebl_32fpu_3"
				FREEBL_LIST="${FREEBL_DEFAULT} libfreebl_32int64_3"
			fi
		else
			if [ "${BIT_NAME}" = "64" ] ; then
				echo "${SCRIPTNAME}: OS not supported for memory leak checking."
				exit 0
			fi
			
			FREEBL_DEFAULT="libfreebl_3"
			FREEBL_LIST="${FREEBL_DEFAULT}"
		fi
		
		RUN_COMMAND_DBG="run_command_dbx"
		PARSE_LOGFILE="parse_logfile_dbx"
		;;
	"Linux")
		VALGRIND=`which valgrind`
		AWK=awk
		
		if [ $? -eq 0 ] ; then
			echo "${SCRIPTNAME}: Valgrind found: ${VALGRIND}"
		else
			echo "${SCRIPTNAME}: Valgrind not found, skipping memory leak checking."
			exit 0
		fi

		FREEBL_DEFAULT="libfreebl_3"
		FREEBL_LIST="${FREEBL_DEFAULT}"
				
		RUN_COMMAND_DBG="run_command_valgrind"
		PARSE_LOGFILE="parse_logfile_valgrind"
		;;
	*)
		echo "${SCRIPTNAME}: OS not supported for memory leak checking."
		exit 0
		;;
	esac

	if [ "${BUILD_OPT}" = "1" ] ; then
		OPT="OPT"
	else 
		OPT="DBG"
	fi

	NSS_DISABLE_UNLOAD="1"
	export NSS_DISABLE_UNLOAD

	SELFSERV_ATTR="-D -p ${PORT} -d ${SERVER_DB} -n ${HOSTADDR} -e ${HOSTADDR}-ec -w nss -c :C001:C002:C003:C004:C005:C006:C007:C008:C009:C00A:C00B:C00C:C00D:C00E:C00F:C010:C011:C012:C013:C014cdefgijklmnvyz -t 5 -V ssl3:tls1.2"
	TSTCLNT_ATTR="-p ${PORT} -h ${HOSTADDR} -c j -f -d ${CLIENT_DB} -w nss -o"
	STRSCLNT_ATTR="-q -p ${PORT} -d ${CLIENT_DB} -w nss -c 1000 -n TestUser ${HOSTADDR}"

	tbytes=0
	tblocks=0
	truns=0
	
	MEMLEAK_DBG=1
	export MEMLEAK_DBG
}

########################### memleak_cleanup ############################
# local shell function to clean up after this script 
########################################################################
memleak_cleanup()
{
	unset MEMLEAK_DBG
	unset NSS_DISABLE_UNLOAD
	
	. ${QADIR}/common/cleanup.sh
}

############################ set_test_mode #############################
# local shell function to set testing mode for server and for client
########################################################################
set_test_mode()
{
	if [ "${server_mode}" = "BYPASS" ] ; then
		echo "${SCRIPTNAME}: BYPASS is ON"
		SERVER_OPTION="-B -s"
		CLIENT_OPTION=""
	elif [ "${client_mode}" = "BYPASS" ] ; then
		echo "${SCRIPTNAME}: BYPASS is ON"
		SERVER_OPTION=""
		CLIENT_OPTION="-B -s"
	else
		echo "${SCRIPTNAME}: BYPASS is OFF"
		SERVER_OPTION=""
		CLIENT_OPTION=""
	fi
	
	if [ "${server_mode}" = "FIPS" ] ; then
		${BINDIR}/modutil -dbdir ${SERVER_DB} -fips true -force
		${BINDIR}/modutil -dbdir ${SERVER_DB} -list
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -fips false -force
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -list
		
		echo "${SCRIPTNAME}: FIPS is ON"
		cipher_list="c d e i j k n v y z"
	elif [ "${client_mode}" = "FIPS" ] ; then
		
		${BINDIR}/modutil -dbdir ${SERVER_DB} -fips false -force
		${BINDIR}/modutil -dbdir ${SERVER_DB} -list
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -fips true -force
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -list
		
		echo "${SCRIPTNAME}: FIPS is ON"
		cipher_list="c d e i j k n v y z"
	else
		${BINDIR}/modutil -dbdir ${SERVER_DB} -fips false -force
		${BINDIR}/modutil -dbdir ${SERVER_DB} -list
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -fips false -force
		${BINDIR}/modutil -dbdir ${CLIENT_DB} -list
		
		echo "${SCRIPTNAME}: FIPS is OFF"
		# ciphers l and m removed, see bug 1136095
		cipher_list=":C001 :C002 :C003 :C004 :C005 :C006 :C007 :C008 :C009 :C00A :C010 :C011 :C012 :C013 :C014 c d e f g i j k n v y z"
	fi
}

############################## set_freebl ##############################
# local shell function to set freebl - sets temporary path for libraries
########################################################################
set_freebl()
{
	if [ "${freebl}" = "${FREEBL_DEFAULT}" ] ; then
		LD_LIBRARY_PATH="${OLD_LIBRARY_PATH}"
		export LD_LIBRARY_PATH
	else
		if [ -d "${TMP_LIBDIR}" ] ; then
			rm -rf ${TMP_LIBDIR}
		fi

		mkdir ${TMP_LIBDIR}
		[ $? -ne 0 ] && html_failed "Create temp directory" && return 1

		cp ${DIST}/${OBJDIR}/lib/*.so ${DIST}/${OBJDIR}/lib/*.chk ${TMP_LIBDIR}
		[ $? -ne 0 ] && html_failed "Copy libraries to temp directory" && return 1
		
		echo "${SCRIPTNAME}: Using ${freebl} instead of ${FREEBL_DEFAULT}"

		mv ${TMP_LIBDIR}/${FREEBL_DEFAULT}.so ${TMP_LIBDIR}/${FREEBL_DEFAULT}.so.orig
		[ $? -ne 0 ] && html_failed "Move ${FREEBL_DEFAULT}.so -> ${FREEBL_DEFAULT}.so.orig" && return 1

		cp ${TMP_LIBDIR}/${freebl}.so ${TMP_LIBDIR}/${FREEBL_DEFAULT}.so
		[ $? -ne 0 ] && html_failed "Copy ${freebl}.so -> ${FREEBL_DEFAULT}.so" && return 1

		mv ${TMP_LIBDIR}/${FREEBL_DEFAULT}.chk ${TMP_LIBDIR}/${FREEBL_DEFAULT}.chk.orig
		[ $? -ne 0 ] && html_failed "Move ${FREEBL_DEFAULT}.chk -> ${FREEBL_DEFAULT}.chk.orig" && return 1

		cp ${TMP_LIBDIR}/${freebl}.chk ${TMP_LIBDIR}/${FREEBL_DEFAULT}.chk
		[ $? -ne 0 ] && html_failed "Copy ${freebl}.chk to temp directory" && return 1

		echo "ls -l ${TMP_LIBDIR}"
		ls -l ${TMP_LIBDIR}

		LD_LIBRARY_PATH="${TMP_LIBDIR}"
		export LD_LIBRARY_PATH
	fi

	return 0
}

############################# clear_freebl #############################
# local shell function to set default library path and clear temporary 
# directory for libraries created by function set_freebl 
########################################################################
clear_freebl()
{
	LD_LIBRARY_PATH="${OLD_LIBRARY_PATH}"
	export LD_LIBRARY_PATH

	if [ -d "${TMP_LIBDIR}" ] ; then
		rm -rf ${TMP_LIBDIR}
	fi
}

############################ run_command_dbx ###########################
# local shell function to run command under dbx tool
########################################################################
run_command_dbx()
{
	COMMAND=$1
	shift
	ATTR=$*
	
	COMMAND=`which ${COMMAND}`
	
	echo "dbxenv follow_fork_mode parent" > ${DBXCMD}
	echo "dbxenv rtc_mel_at_exit verbose" >> ${DBXCMD}
	echo "dbxenv rtc_biu_at_exit verbose" >> ${DBXCMD}
	echo "check -memuse -match 16 -frames 16" >> ${DBXCMD}
	echo "run ${ATTR}" >> ${DBXCMD}
	
	export NSS_DISABLE_ARENA_FREE_LIST=1
	
	echo "${SCRIPTNAME}: -------- Running ${COMMAND} under DBX:"
	echo "${DBX} ${COMMAND}"
	echo "${SCRIPTNAME}: -------- DBX commands:"
	cat ${DBXCMD}
	
	( ${DBX} ${COMMAND} < ${DBXCMD} > ${DBXOUT} 2> ${DBXERR} )
	grep -v Reading ${DBXOUT} 1>&2
	cat ${DBXERR}
	
	unset NSS_DISABLE_ARENA_FREE_LIST
	
	grep "exit code is" ${DBXOUT}
	grep "exit code is 0" ${DBXOUT} > /dev/null
	return $?
}

######################### run_command_valgrind #########################
# local shell function to run command under valgrind tool
########################################################################
run_command_valgrind()
{
	COMMAND=$1
	shift
	ATTR=$*
	
	export NSS_DISABLE_ARENA_FREE_LIST=1
	
	echo "${SCRIPTNAME}: -------- Running ${COMMAND} under Valgrind:"
	echo "${VALGRIND} --tool=memcheck --leak-check=yes --show-reachable=yes --partial-loads-ok=yes --leak-resolution=high --num-callers=50 ${COMMAND} ${ATTR}"
	echo "Running: ${COMMAND} ${ATTR}" 1>&2
	${VALGRIND} --tool=memcheck --leak-check=yes --show-reachable=yes --partial-loads-ok=yes --leak-resolution=high --num-callers=50 ${COMMAND} ${ATTR} 1>&2
	ret=$?
	echo "==0=="
	
	unset NSS_DISABLE_ARENA_FREE_LIST
	
	return $ret
}

############################# run_selfserv #############################
# local shell function to start selfserv
########################################################################
run_selfserv()
{
	echo "PATH=${PATH}"
	echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
	echo "${SCRIPTNAME}: -------- Running selfserv:"
	echo "selfserv ${SELFSERV_ATTR}"
	${BINDIR}/selfserv ${SELFSERV_ATTR}
	ret=$?
	if [ $ret -ne 0 ]; then
		html_failed "${LOGNAME}: Selfserv"
		echo "${SCRIPTNAME} ${LOGNAME}: " \
			"Selfserv produced a returncode of ${ret} - FAILED"
	fi
}

########################### run_selfserv_dbg ###########################
# local shell function to start selfserv under debug tool
########################################################################
run_selfserv_dbg()
{
	echo "PATH=${PATH}"
	echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
	${RUN_COMMAND_DBG} ${BINDIR}/selfserv ${SERVER_OPTION} ${SELFSERV_ATTR}
	ret=$?
	if [ $ret -ne 0 ]; then
		html_failed "${LOGNAME}: Selfserv"
		echo "${SCRIPTNAME} ${LOGNAME}: " \
			"Selfserv produced a returncode of ${ret} - FAILED"
	fi
}

############################# run_strsclnt #############################
# local shell function to run strsclnt for all ciphers and send stop
# command to selfserv over tstclnt
########################################################################
run_strsclnt()
{
	for cipher in ${cipher_list}; do
		VMIN="ssl3"
		VMAX="tls1.2"
		case "${cipher}" in
		f|g)
			# TLS 1.1 disallows export cipher suites.
			VMAX="tls1.0"
			;;
		esac
		ATTR="${STRSCLNT_ATTR} -C ${cipher} -V ${VMIN}:${VMAX}"
		echo "${SCRIPTNAME}: -------- Trying cipher ${cipher}:"
		echo "strsclnt ${ATTR}"
		${BINDIR}/strsclnt ${ATTR}
		ret=$?
		if [ $ret -ne 0 ]; then
			html_failed "${LOGNAME}: Strsclnt with cipher ${cipher}"
			echo "${SCRIPTNAME} ${LOGNAME}: " \
				"Strsclnt produced a returncode of ${ret} - FAILED"
		fi
	done
	
	ATTR="${TSTCLNT_ATTR} -V ssl3:tls1.2"
	echo "${SCRIPTNAME}: -------- Stopping server:"
	echo "tstclnt ${ATTR} < ${REQUEST_FILE}"
	${BINDIR}/tstclnt ${ATTR} < ${REQUEST_FILE}
	ret=$?
	if [ $ret -ne 0 ]; then
		html_failed "${LOGNAME}: Tstclnt"
		echo "${SCRIPTNAME} ${LOGNAME}: " \
			"Tstclnt produced a returncode of ${ret} - FAILED"
	fi
	
	sleep 20
	kill $(jobs -p) 2> /dev/null
}

########################### run_strsclnt_dbg ###########################
# local shell function to run strsclnt under debug tool for all ciphers 
# and send stop command to selfserv over tstclnt
########################################################################
run_strsclnt_dbg()
{
	for cipher in ${cipher_list}; do
		VMIN="ssl3"
		VMAX="tls1.2"
		case "${cipher}" in
		f|g)
			# TLS 1.1 disallows export cipher suites.
			VMAX="tls1.0"
			;;
		esac
		ATTR="${STRSCLNT_ATTR} -C ${cipher} -V ${VMIN}:${VMAX}"
		${RUN_COMMAND_DBG} ${BINDIR}/strsclnt ${CLIENT_OPTION} ${ATTR}
		ret=$?
		if [ $ret -ne 0 ]; then
			html_failed "${LOGNAME}: Strsclnt with cipher ${cipher}"
			echo "${SCRIPTNAME} ${LOGNAME}: " \
				"Strsclnt produced a returncode of ${ret} - FAILED"
		fi
	done
	
	ATTR="${TSTCLNT_ATTR} -V ssl3:tls1.2"
	echo "${SCRIPTNAME}: -------- Stopping server:"
	echo "tstclnt ${ATTR} < ${REQUEST_FILE}"
	${BINDIR}/tstclnt ${ATTR} < ${REQUEST_FILE}
	ret=$?
	if [ $ret -ne 0 ]; then
		html_failed "${LOGNAME}: Tstclnt"
		echo "${SCRIPTNAME} ${LOGNAME}: " \
			"Tstclnt produced a returncode of ${ret} - FAILED"
	fi
	
	kill $(jobs -p) 2> /dev/null
}

stat_clear()
{
	stat_minbytes=9999999
	stat_maxbytes=0
	stat_minblocks=9999999
	stat_maxblocks=0
	stat_bytes=0
	stat_blocks=0
	stat_runs=0
}

stat_add()
{
	read hash lbytes bytes_str lblocks blocks_str in_str lruns runs_str \
		minbytes minbytes_str maxbytes maxbytes_str minblocks \
		minblocks_str maxblocks maxblocks_str rest < ${TMP_COUNT} 
	rm ${TMP_COUNT}
	
	tbytes=`expr ${tbytes} + ${lbytes}`
	tblocks=`expr ${tblocks} + ${lblocks}`
	truns=`expr ${truns} + ${lruns}`
	
	if [ ${stat_minbytes} -gt ${minbytes} ]; then
		stat_minbytes=${minbytes}
	fi
			
	if [ ${stat_maxbytes} -lt ${maxbytes} ]; then
		stat_maxbytes=${maxbytes}
	fi
			
	if [ ${stat_minblocks} -gt ${minblocks} ]; then
		stat_minblocks=${minblocks}
	fi
			
	if [ ${stat_maxblocks} -lt ${maxblocks} ]; then
		stat_maxblocks=${maxblocks}
	fi
			
	stat_bytes=`expr ${stat_bytes} + ${lbytes}`
	stat_blocks=`expr ${stat_blocks} + ${lblocks}`
	stat_runs=`expr ${stat_runs} + ${lruns}`
}

stat_print()
{
	if [ ${stat_runs} -gt 0 ]; then
		stat_avgbytes=`expr "${stat_bytes}" / "${stat_runs}"`
		stat_avgblocks=`expr "${stat_blocks}" / "${stat_runs}"`
		
		echo
		echo "$1 statistics:"
		echo "Leaked bytes: ${stat_minbytes} min, ${stat_avgbytes} avg, ${stat_maxbytes} max"
		echo "Leaked blocks: ${stat_minblocks} min, ${stat_avgblocks} avg, ${stat_maxblocks} max"
		echo "Total runs: ${stat_runs}"
		echo
	fi
}

########################## run_ciphers_server ##########################
# local shell function to test server part of code (selfserv)
########################################################################
run_ciphers_server()
{
	html_head "Memory leak checking - server"
	
	stat_clear
	
	client_mode="NORMAL"	
	for server_mode in ${MODE_LIST}; do
		set_test_mode
		
		for freebl in ${FREEBL_LIST}; do
			set_freebl || continue
			
			LOGNAME=server-${BIT_NAME}-${freebl}-${server_mode}
			LOGFILE=${LOGDIR}/${LOGNAME}.log
			echo "Running ${LOGNAME}"
			
			(
			    run_selfserv_dbg 2>> ${LOGFILE} &
			    sleep 5
			    run_strsclnt
			)
			
			sleep 20
			clear_freebl
			
			log_parse
			ret=$?
			
			html_msg ${ret} 0 "${LOGNAME}" "produced a returncode of $ret, expected is 0"
		done
	done
	
	stat_print "Selfserv"
	
	html "</TABLE><BR>"
}

########################## run_ciphers_client ##########################
# local shell function to test client part of code (strsclnt)
########################################################################
run_ciphers_client()
{
	html_head "Memory leak checking - client"
	
	stat_clear
	
	server_mode="NORMAL"
	for client_mode in ${MODE_LIST}; do
		set_test_mode
		
		for freebl in ${FREEBL_LIST}; do
			set_freebl || continue
			
			LOGNAME=client-${BIT_NAME}-${freebl}-${client_mode}
			LOGFILE=${LOGDIR}/${LOGNAME}.log
			echo "Running ${LOGNAME}"
			
			(
			    run_selfserv &
			    sleep 5
			    run_strsclnt_dbg 2>> ${LOGFILE}
			)
			
			sleep 20
			clear_freebl
			
			log_parse
			ret=$?
			html_msg ${ret} 0 "${LOGNAME}" "produced a returncode of $ret, expected is 0"
		done
	done
	
	stat_print "Strsclnt"
	
	html "</TABLE><BR>"
}

########################## parse_logfile_dbx ###########################
# local shell function to parse and process logs from dbx
########################################################################
parse_logfile_dbx()
{
	${AWK} '
	BEGIN {
		in_mel = 0
		mel_line = 0
		bytes = 0
		lbytes = 0
		minbytes = 9999999
		maxbytes = 0
		blocks = 0
		lblocks = 0
		minblocks = 9999999
		maxblocks = 0
		runs = 0
		stack_string = ""
		bin_name = ""
	}
	/Memory Leak \(mel\):/ ||
	/Possible memory leak -- address in block \(aib\):/ ||
	/Block in use \(biu\):/ {
		in_mel = 1
		stack_string = ""
		next
	}
	in_mel == 1 && /^$/ {
		print bin_name stack_string
		in_mel = 0
		mel_line = 0
		next
	}
	in_mel == 1 {
		mel_line += 1
	}
	/Found leaked block of size/ {
		bytes += $6
		blocks += 1
		next
	}
	/Found .* leaked blocks/ {
		bytes += $8
		blocks += $2
		next
	}
	/Found block of size/ {
		bytes += $5
		blocks += 1
		next
	}
	/Found .* blocks totaling/ {
		bytes += $5
		blocks += $2
		next
	}
	mel_line > 2 {
		gsub(/\(\)/, "")
		new_line = $2
		stack_string = "/" new_line stack_string
		next
	}
	/^Running: / {
		bin_name = $2
		next
	}
	/execution completed/ {
		runs += 1
		lbytes += bytes
		minbytes = (minbytes < bytes) ? minbytes : bytes
		maxbytes = (maxbytes > bytes) ? maxbytes : bytes
		bytes = 0
		lblocks += blocks
		minblocks = (minblocks < blocks) ? minblocks : blocks
		maxblocks = (maxblocks > blocks) ? maxblocks : blocks
		blocks = 0
		next
	}
	END {
		print "# " lbytes " bytes " lblocks " blocks in " runs " runs " \
		minbytes " minbytes " maxbytes " maxbytes " minblocks " minblocks " \
		maxblocks " maxblocks " > "/dev/stderr"
	}' 2> ${TMP_COUNT}
	
	stat_add
}

######################## parse_logfile_valgrind ########################
# local shell function to parse and process logs from valgrind
########################################################################
parse_logfile_valgrind()
{
	${AWK} '
	BEGIN {
		in_mel = 0
		in_sum = 0
		bytes = 0
		lbytes = 0
		minbytes = 9999999
		maxbytes = 0
		blocks = 0
		lblocks = 0
		minblocks = 9999999
		maxblocks = 0
		runs = 0
		stack_string = ""
		bin_name = "" 
	}
	!/==[0-9]*==/ { 
		if ( $1 == "Running:" ) 
			bin_name = $2
			bin_nf = split(bin_name, bin_fields, "/")
			bin_name = bin_fields[bin_nf]
		next
	}
	/blocks are/ {
		in_mel = 1
		stack_string = ""
		next
	}
	/LEAK SUMMARY/ {
		in_sum = 1
		next
	}
	/^==[0-9]*== *$/ { 
		if (in_mel)
			print bin_name stack_string
		if (in_sum) {
			runs += 1
			lbytes += bytes
			minbytes = (minbytes < bytes) ? minbytes : bytes
			maxbytes = (maxbytes > bytes) ? maxbytes : bytes
			bytes = 0
			lblocks += blocks
			minblocks = (minblocks < blocks) ? minblocks : blocks
			maxblocks = (maxblocks > blocks) ? maxblocks : blocks
			blocks = 0
		}
		in_sum = 0
		in_mel = 0
		next
	}
	in_mel == 1 {	
		new_line = $4
		if ( new_line == "(within")
			new_line = "*"
		stack_string = "/" new_line stack_string
	}
	in_sum == 1 {
		for (i = 2; i <= NF; i++) {
			if ($i == "bytes") {
				str = $(i - 1)
				gsub(",", "", str)
				bytes += str
			}
			if ($i == "blocks.") {
				str = $(i - 1)
				gsub(",", "", str)
				blocks += str
			}
		}
	}
	END {
		print "# " lbytes " bytes " lblocks " blocks in " runs " runs " \
		minbytes " minbytes " maxbytes " maxbytes " minblocks " minblocks " \
		maxblocks " maxblocks " > "/dev/stderr"
	}' 2> ${TMP_COUNT}
	
	stat_add
}

############################# check_ignored ############################
# local shell function to check all stacks if they are not ignored
########################################################################
check_ignored()
{
	${AWK} -F/ '
	BEGIN {
		ignore = "'${IGNORED_STACKS}'"
		# read in the ignore file
		BUGNUM = ""
		count = 0
		new = 0
		while ((getline line < ignore) > 0)  {
			if (line ~ "^#[0-9]+") {
				BUGNUM = line
			} else if (line ~ "^#") {
				continue
			} else if (line == "") {
				continue
			} else {
				bugnum_array[count] = BUGNUM
				# Create a regular expression for the ignored stack:
				# replace * with % so we can later replace them with regular expressions
				# without messing up everything (the regular expressions contain *)
				gsub("\\*", "%", line)
				# replace %% with .*
				gsub("%%", ".*", line)
				# replace % with [^/]*
				gsub("%", "[^/]*", line)
				# add ^ at the beginning
				# add $ at the end
				line_array[count] = "^" line "$"
				count++
			}
		}
	}
	{
		match_found = 0
		# Look for matching ignored stack
		for (i = 0; i < count; i++) {
			if ($0 ~ line_array[i]) {
				# found a match
				match_found = 1
				bug_found = bugnum_array[i]
				break
			}
		}
		# Process result
		if (match_found == 1 ) {
				if (bug_found != "") {
					print "IGNORED STACK (" bug_found "): " $0
				} else {
					print "IGNORED STACK: " $0
				}
		} else {
				print "NEW STACK: " $0
				new = 1
		}
	}
	END {
		exit new
	}'
	ret=$?
	return $ret
}

############################### parse_log ##############################
# local shell function to parse log file
########################################################################
log_parse()
{
	${PARSE_LOGFILE} < ${LOGFILE} > ${TMP_STACKS}
	echo "${SCRIPTNAME}: Processing log ${LOGNAME}:" > ${TMP_SORTED}
	cat ${TMP_STACKS} | sort -u | check_ignored >> ${TMP_SORTED}
	ret=$?
	echo >> ${TMP_SORTED}
	
	cat ${TMP_SORTED} | tee -a ${FOUNDLEAKS}
	rm ${TMP_STACKS} ${TMP_SORTED}
	
	return ${ret}
}

############################## cnt_total ###############################
# local shell function to count total leaked bytes
########################################################################
cnt_total()
{
	echo ""
	echo "TinderboxPrint:${OPT} Lk bytes: ${tbytes}"
	echo "TinderboxPrint:${OPT} Lk blocks: ${tblocks}"
	echo "TinderboxPrint:${OPT} # of runs: ${truns}"
	echo ""
}

############################### run_ocsp ###############################
# local shell function to run ocsp tests
########################################################################
run_ocsp()
{
	stat_clear
	
	cd ${QADIR}/iopr
	. ./ocsp_iopr.sh
	ocsp_iopr_run
	
	stat_print "Ocspclnt"
}

############################## run_chains ##############################
# local shell function to run PKIX certificate chains tests
########################################################################
run_chains()
{
    stat_clear

    LOGNAME="chains"
    LOGFILE=${LOGDIR}/chains.log

    . ${QADIR}/chains/chains.sh

    stat_print "Chains"
}

############################## run_chains ##############################
# local shell function to run memory leak tests
#
# NSS_MEMLEAK_TESTS - list of tests to run, if not defined before,
# then is redefined to default list 
########################################################################
memleak_run_tests()
{
    nss_memleak_tests="ssl_server ssl_client chains ocsp"
    NSS_MEMLEAK_TESTS="${NSS_MEMLEAK_TESTS:-$nss_memleak_tests}"

    for MEMLEAK_TEST in ${NSS_MEMLEAK_TESTS}
    do
        case "${MEMLEAK_TEST}" in
        "ssl_server")
            run_ciphers_server
            ;;
        "ssl_client")
            run_ciphers_client
            ;;
        "chains")
            run_chains
            ;;
        "ocsp")
            run_ocsp
            ;;
        esac
    done
}

################################# main #################################

memleak_init
memleak_run_tests
cnt_total
memleak_cleanup

