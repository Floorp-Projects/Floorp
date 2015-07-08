#!/bin/bash
###########################################################################
# This requires coverage and nosetests:
#
#  pip install -r requirements.txt
#
# test_base_vcs_mercurial.py requires hg >= 1.6.0 with mq, rebase, share
# extensions to fully test.
###########################################################################

COVERAGE_ARGS="--omit='/usr/*,/opt/*'"
OS_TYPE='linux'
uname -v | grep -q Darwin
if [ $? -eq 0 ] ; then
  OS_TYPE='osx'
  COVERAGE_ARGS="--omit='/Library/*,/usr/*,/opt/*'"
fi
uname -s | egrep -q MINGW32   # Cygwin will be linux in this case?
if [ $? -eq 0 ] ; then
  OS_TYPE='windows'
fi
NOSETESTS=`env which nosetests`

echo "### Finding mozharness/ .py files..."
files=`find mozharness -name [a-z]\*.py`
if [ $OS_TYPE == 'windows' ] ; then
  MOZHARNESS_PY_FILES=""
  for f in $files; do
    file $f | grep -q "Assembler source"
    if [ $? -ne 0 ] ; then
      MOZHARNESS_PY_FILES="$MOZHARNESS_PY_FILES $f"
    fi
  done
else
  MOZHARNESS_PY_FILES=$files
fi
echo "### Finding scripts/ .py files..."
files=`find scripts -name [a-z]\*.py`
if [ $OS_TYPE == 'windows' ] ; then
  SCRIPTS_PY_FILES=""
  for f in $files; do
    file $f | grep -q "Assembler source"
    if [ $? -ne 0 ] ; then
      SCRIPTS_PY_FILES="$SCRIPTS_PY_FILES $f"
    fi
  done
else
  SCRIPTS_PY_FILES=$files
fi
export PYTHONPATH=`env pwd`:$PYTHONPATH

echo "### Running pyflakes"
pyflakes $MOZHARNESS_PY_FILES $SCRIPTS_PY_FILES | grep -v "local variable 'url' is assigned to" | grep -v "redefinition of unused 'json'" | egrep -v "mozharness/mozilla/testing/mozpool\.py.*undefined name 'requests'"

echo "### Running pylint"
pylint -E -e F -f parseable $MOZHARNESS_PY_FILES $SCRIPTS_PY_FILES 2>&1 | egrep -v '(No config file found, using default configuration|Instance of .* has no .* member|Unable to import .devicemanager|Undefined variable .DMError|Module .hashlib. has no .sha512. member)'

rm -rf build logs
if [ $OS_TYPE != 'windows' ] ; then
  echo "### Testing non-networked unit tests"
  coverage run -a --branch $COVERAGE_ARGS $NOSETESTS test/test_*.py
  echo "### Running *.py [--list-actions]"
  for filename in $MOZHARNESS_PY_FILES; do
    coverage run -a --branch $COVERAGE_ARGS $filename
  done
  for filename in $SCRIPTS_PY_FILES ; do
    coverage run -a --branch $COVERAGE_ARGS $filename --list-actions > /dev/null
  done
  echo "### Running scripts/configtest.py --log-level warning"
  coverage run -a --branch $COVERAGE_ARGS scripts/configtest.py --log-level warning

  echo "### Creating coverage html"
  coverage html $COVERAGE_ARGS -d coverage.new
  if [ -e coverage ] ; then
      mv coverage coverage.old
      mv coverage.new coverage
      rm -rf coverage.old
  else
      mv coverage.new coverage
  fi
else
  echo "### Running nosetests..."
  nosetests test/
fi
rm -rf build logs
