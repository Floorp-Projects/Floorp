#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

PYTHON=$1

# Store the current working directory so we can change back into it after
# preparing the Marionette virtualenv
CWD=$(pwd)

if [ -z "${PYTHON}" ]
then
    echo "No python found"
    exit 1
fi

# Determine the absolute path of the Marionette home folder
MARIONETTE_HOME=$(cd `dirname $BASH_SOURCE`; dirname `pwd`)
echo "Detected Marionette home in $MARIONETTE_HOME"

# If a GECKO_OBJDIR environemnt variable exists, we will create the Python
# virtual envirnoment there. Otherwise we create it in the PWD.
VENV_DIR="marionette_venv"
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
  . $VENV_DIR/bin/activate
else
  echo "Creating a virtual environment in $VENV_DIR"
  curl https://raw.github.com/pypa/virtualenv/develop/virtualenv.py | ${PYTHON} - $VENV_DIR
  . $VENV_DIR/bin/activate

  # set up mozbase
  git clone git://github.com/mozilla/mozbase.git $VENV_DIR/mozbase
  python $VENV_DIR/mozbase/setup_development.py
fi

# Updating the marionette_client needs us to change into its package folder.
# Otherwise the call to setup.py will hang
cd $MARIONETTE_HOME
python setup.py develop
cd $CWD

# pop off the python parameter
shift

python $MARIONETTE_HOME/marionette/runtests.py $@
