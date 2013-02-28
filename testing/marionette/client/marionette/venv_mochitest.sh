#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

VIRTUAL_ENV_VERSION="49f40128a9ca3824ebf253eca408596e135cf893"

PYTHON=$1

# Store the current working directory so we can change back into it after
# preparing the Marionette virtualenv
CWD=$(pwd)

if [ -z "${PYTHON}" ]
then
    echo "No python found"
    exit 1
fi

# Determine the absolute path of our location.
MARIONETTE_HOME=$(cd `dirname $0`/..; pwd)
echo "Detected Marionette home in $MARIONETTE_HOME"

# If a GECKO_OBJDIR environemnt variable exists, we will create the Python
# virtual envirnoment there. Otherwise we create it in the PWD.
VENV_DIR="mochitest_venv"
if [ -z $GECKO_OBJDIR ]
then
    VENV_DIR="$MARIONETTE_HOME/$VENV_DIR"
else
    VENV_DIR="$GECKO_OBJDIR/$VENV_DIR"
fi

# Check if environment exists, if not, create a virtualenv:
if [ -d $VENV_DIR ]
then
  echo "Using virtual environment in $VENV_DIR"
else
  echo "Creating a virtual environment in $VENV_DIR"
  curl https://raw.github.com/pypa/virtualenv/${VIRTUAL_ENV_VERSION}/virtualenv.py | ${PYTHON} - $VENV_DIR
fi
. $VENV_DIR/bin/activate

# Updating the marionette_client needs us to change into its package folder.
# Otherwise the call to setup.py will hang
cd $MARIONETTE_HOME
python setup.py develop
cd $CWD

# pop off the python parameter
shift

TEST_PWD=${TEST_PWD:-$GECKO_OBJDIR/_tests/testing/mochitest}
cd $TEST_PWD

set -x
python runtestsb2g.py $@
