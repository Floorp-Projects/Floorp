#! /bin/sh  
#
########################################################################
#
# mozilla/security/nss/tests/perf
#
# script run from the nightly NSS QA to measure nss performance
#
########################################################################
#
. ../common/init.sh
CURDIR=`pwd`

#echo "<HTML><BODY>" >> ${RESULTS}

#SONMI_DEBUG=ON	# for now save all tmp files


PERFDIR=${HOSTDIR}/perf

mkdir -p ${PERFDIR}

RSAPERF_OUT=`rsaperf -i 300 -s -n none`
RSAPERF_OUT=`echo $RSAPERF_OUT | sed -e "s/^/RSAPERF: $OBJDIR /" \
		-e 's/microseconds/us/' -e 's/milliseconds/ms/' -e 's/seconds/s/' \
		-e 's/ minutes, and /_min_/'`

echo "$RSAPERF_OUT"
#export RSAPERF_OUT
#
#perl -e '

#@rsaperf=split(/ /, $ENV{RSAPERF_OUT});

#echo "${RSAPERF_OUT}" | read IT_NUM T1 T2 TOT_TIM TOT_TIM_U \
	#T3 T4 T5 AVRG_TIM AVRG_TIM_U

#300 iterations in 8.881 seconds one operation every 29606 microseconds
