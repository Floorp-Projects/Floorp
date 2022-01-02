#! /bin/bash  
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/perf/perf.sh
#
# script run from the nightly NSS QA to measure nss performance
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
########################################################################

############################## perf_init ##############################
# local shell function to initialize this script
########################################################################

perf_init()
{
  SCRIPTNAME="perf.sh"
  if [ -z "${INIT_SOURCED}" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="perf.sh"
  PERFDIR=${HOSTDIR}/perf
  mkdir -p ${PERFDIR}
}

perf_init
cd ${PERFDIR}
RSAPERF_OUT=`${BINDIR}/rsaperf -i 300 -s -n none`
RSAPERF_OUT=`echo $RSAPERF_OUT | sed \
                -e "s/^/RSAPERF: $OBJDIR /" \
                -e 's/microseconds/us/' \
                -e 's/milliseconds/ms/' \
                -e 's/seconds/s/' \
                -e 's/ minutes, and /_min_/'`

echo "$RSAPERF_OUT"



#FIXME
#export RSAPERF_OUT
#
#perl -e '

#@rsaperf=split(/ /, $ENV{RSAPERF_OUT});

#echo "${RSAPERF_OUT}" | read IT_NUM T1 T2 TOT_TIM TOT_TIM_U \
    #T3 T4 T5 AVRG_TIM AVRG_TIM_U

#300 iterations in 8.881 seconds one operation every 29606 microseconds
