#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mpi_init()
{
  SCRIPTNAME="mpi.sh"
  if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ] ; then
      cd ../common
      . ./init.sh
  fi
  SCRIPTNAME="mpi.sh"
  html_head "MPI tests"
}

mpi_cleanup()
{
  html "</TABLE><BR>"
  cd ${QADIR}
  . common/cleanup.sh
}

mpi_init
tests=($(mpi_tests list | awk '{print $1}'))
for test in "${tests[@]}"
do
  OUT=$(mpi_tests $test 2>&1)
  [ ! -z "$OUT" ] && echo "$OUT"
  OUT=`echo $OUT | grep -i 'error\|Assertion failure'`

  if [ -n "$OUT" ] ; then
    html_failed "mpi $test test"
  else
    html_passed "mpi $test test"
  fi
done

mpi_cleanup
