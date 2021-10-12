#!/bin/bash
set -ex
set -o pipefail
# This ugly hack is a cross-platform (Linux/Mac/Windows+MSYS) way to get the
# absolute path to the directory containing this script
pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
SCRIPTS_DIR="$MY_DIR/.."
PYTHON='./mach python'
VERIFY_CONFIG="$MOZ_FETCHES_DIR/update-verify.cfg"

while [ "$#" -gt 0 ]; do
  case $1 in
      # Parse total-chunks
      --total-chunks=*) chunks="${1#*=}"; shift 1;;
      --total-chunks) chunks="${2}"; shift 2;;

      # Parse this-chunk
      --this-chunk=*) thisChunk="${1#*=}"; shift 1;;
      --this-chunk) thisChunk="${2}"; shift 2;;

      # Stop if other parameters are sent
      *) echo "Unknown parameter: ${1}"; exit 1;;
  esac
done

# Validate parameters
if [ -z "${chunks}" ]; then echo "Required parameter: --total-chunks"; exit 1; fi
if [ -z "${thisChunk}" ]; then echo "Required parameter: --this-chunk"; exit 1; fi

# release promotion
if [ -n "$CHANNEL" ]; then
  EXTRA_PARAMS="--verify-channel $CHANNEL"
else
  EXTRA_PARAMS=""
fi
$PYTHON $MY_DIR/chunked-verify.py --chunks $chunks --this-chunk $thisChunk \
--verify-config $VERIFY_CONFIG --diff-summary $PWD/diff-summary.log $EXTRA_PARAMS \
2>&1 | tee $SCRIPTS_DIR/../verify_log.txt

print_failed_msg()
{
  echo "-------------------------"
  echo "This run has failed, see the above log"
  echo
  return 1
}

print_warning_msg()
{
  echo "-------------------------"
  echo "This run has warnings, see the above log"
  echo
  return 2
}

set +x

echo "Scanning log for failures and warnings"
echo "--------------------------------------"

# Test for a failure, note we are set -e.
# Grep returns 0 on a match and 1 on no match
# Testing for failures first is important because it's OK to to mark as failed
# when there's failures+warnings, but not OK to mark as warnings in the same
# situation.
( ! grep 'TEST-UNEXPECTED-FAIL:' $SCRIPTS_DIR/../verify_log.txt ) || print_failed_msg
( ! grep 'WARN:' $SCRIPTS_DIR/../verify_log.txt ) || print_warning_msg

echo "-------------------------"
echo "All is well"
