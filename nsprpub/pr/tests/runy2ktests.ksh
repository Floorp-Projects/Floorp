#!/bin/ksh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# runy2ktests.ksh
#	Set system clock to Y2K dates of interest and run the Y2K tests.
#	Needs root/administrator privilege
#
# WARNING: Because this script needs to be run with root/administrator
#	privilege, thorough understanding of the script and extreme
#	caution are urged.
#

#
# SECTION I
#	Define variables
#

SYSTEM_INFO=`uname -a`
OS_ARCH=`uname -s`
if [ $OS_ARCH = "Windows_NT" ] || [ $OS_ARCH = "Windows_95" ]
then
	NULL_DEVICE=nul
else
	NULL_DEVICE=/dev/null
fi

#
# Test dates for NSPR Y2K tests
#
Y2KDATES="	123123591998.55
			090923591999.55
			123123591999.55
			022823592000.55
			022923592000.55
			123123592000.55"

Y2KDATES_AIX="	12312359.5598
				09092359.5599
				12312359.5599
				02282359.5500
				02292359.5500
				12312359.5500"

Y2KDATES_HPUX="	123123591998
				090923591999
				123123591999
				022823592000
				022923592000
				123123592000"

Y2KDATES_MKS="	1231235998.55
				0909235999.55
				1231235999.55
				0228235900.55
				0229235900.55
				1231235900.55"

#
# NSPR Y2K tests
#
Y2KTESTS="
y2k		\n
y2ktmo	\n
y2k		\n
../runtests.ksh"

Y2KTESTS_HPUX="
y2k			\n
y2ktmo -l 60\n
y2k			\n
../runtests.ksh"

#
# SECTION II
#	Define functions
#

save_date()
{
	case $OS_ARCH in
	AIX)
		SAVED_DATE=`date "+%m%d%H%M.%S%y"`
	;;
	HP-UX)
		SAVED_DATE=`date "+%m%d%H%M%Y"`
	;;
	Windows_NT)
		SAVED_DATE=`date "+%m%d%H%M%y.%S"`
	;;
	Windows_95)
		SAVED_DATE=`date "+%m%d%H%M%y.%S"`
	;;
	*)
		SAVED_DATE=`date "+%m%d%H%M%Y.%S"`
	;;
	esac
}

set_date()
{
	case $OS_ARCH in
	Windows_NT)
#
# The date command in MKS Toolkit releases 5.1 and 5.2
# uses the current DST status for the date we want to
# set the system clock to.  However, the DST status for
# that date may be different from the current DST status.
# We can work around this problem by invoking the date
# command with the same date twice.
#
		date "$1" > $NULL_DEVICE
		date "$1" > $NULL_DEVICE
	;;
	*)
		date "$1" > $NULL_DEVICE
	;;
	esac
}

restore_date()
{
	set_date "$SAVED_DATE"
}

savedate()
{
	case $OS_ARCH in
	AIX)
		SAVED_DATE=`date "+%m%d%H%M.%S%y"`
	;;
	HP-UX)
		SAVED_DATE=`date "+%m%d%H%M%Y"`
	;;
	Windows_NT)
		SAVED_DATE=`date "+%m%d%H%M%y.%S"`
	;;
	Windows_95)
		SAVED_DATE=`date "+%m%d%H%M%y.%S"`
	;;
	*)
		SAVED_DATE=`date "+%m%d%H%M%Y.%S"`
	;;
	esac
}

set_y2k_test_parameters()
{
#
# set dates
#
	case $OS_ARCH in
	AIX)
		DATES=$Y2KDATES_AIX
	;;
	HP-UX)
		DATES=$Y2KDATES_HPUX
	;;
	Windows_NT)
		DATES=$Y2KDATES_MKS
	;;
	Windows_95)
		DATES=$Y2KDATES_MKS
	;;
	*)
		DATES=$Y2KDATES
	;;
	esac

#
# set tests
#
	case $OS_ARCH in
	HP-UX)
		TESTS=$Y2KTESTS_HPUX
	;;
	*)
		TESTS=$Y2KTESTS
	;;
	esac
}

#
# runtests:
#	-	runs each test in $TESTS after setting the
#		system clock to each date in $DATES
#

runtests()
{
for newdate in ${DATES}
do
	set_date $newdate
	echo $newdate
	echo "BEGIN\t\t\t`date`"
	echo "Date\t\t\t\t\tTest\t\t\tResult"
	echo $TESTS | while read prog
	do
		echo "`date`\t\t\c"
		echo "$prog\c"
		./$prog >> ${LOGFILE} 2>&1
		if [ 0 = $? ] ; then
			echo "\t\t\tPassed";
		else
			echo "\t\t\tFAILED";
		fi;
	done
	echo "END\t\t\t`date`\n"
done

}

#
# SECTION III
#	Run tests
#

LOGFILE=${NSPR_TEST_LOGFILE:-$NULL_DEVICE}
OBJDIR=`basename $PWD`
echo "\nNSPR Year 2000 Test Results - $OBJDIR\n"
echo "SYSTEM:\t\t\t${SYSTEM_INFO}"
echo "NSPR_TEST_LOGFILE:\t${LOGFILE}\n"


save_date

#
# Run NSPR Y2k and standard tests
#

set_y2k_test_parameters
runtests

restore_date
