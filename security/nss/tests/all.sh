#!/bin/sh
#
# Run all our tests
#
CURDIR=`pwd`
TESTS="sdr ssl cipher smime"
cd common
. ./init.sh
export MOZILLA_ROOT
export COMMON
export DIST
export SECURITY_ROOT
export TESTDIR
export OBJDIR
export HOSTDIR

LOGFILE=${HOSTDIR}/output.log
export LOGFILE
touch ${LOGFILE}

if [ -z "O_CRON" -o "$O_CRON" != "ON" ]
then
	tail -f ${LOGFILE}  &
	TAILPID=$!
	trap "kill ${TAILPID}; exit" 2 
fi

for i in ${TESTS}
do
	echo "Running Tests for $i"
#
# All tells the test suite to run through all their tests.
# file tells the test suite that the output is going to a log, so any
#  forked() children need to redirect their output to prevent them from
#  being over written.

	(cd ${CURDIR}/$i ; ./${i}.sh all file >> ${LOGFILE} 2>&1)
#	cd ${CURDIR}/$i ; ./${i}.sh 
done
if [ -z "O_CRON" -o "$O_CRON" != "ON" ]
then
	kill ${TAILPID}
fi
