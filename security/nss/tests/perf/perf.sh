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
RSAPERF_OUT=`rsaperf -i 300 -s -n none`
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
