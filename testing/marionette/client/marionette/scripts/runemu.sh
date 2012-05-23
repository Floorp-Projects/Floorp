#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Marionette requires Python 2.7, so the first parameter to this script
# should be the path to that.
PYTHON=$1

if [ -z "${PYTHON}" ]
then
    echo "No python found"
    exit 1
fi

# Determine the absolute path of our location.
echo $0
echo `dirname $0`
echo $PWD
SCRIPTS_HOME=`dirname $0`
cd $SCRIPTS_HOME
cd ..
MARIONETTE_HOME=`dirname $PWD`
echo "Detected Marionette home in $MARIONETTE_HOME"

# If a GECKO_OBJDIR environemnt variable exists, we will create the Python
# virtual envirnoment there. Otherwise we create it in the PWD.
VENV_DIR="runemu_venv"
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
  cd $VENV_DIR
  . bin/activate
else
  echo "Creating a virtual environment in $VENV_DIR"
  curl https://raw.github.com/pypa/virtualenv/develop/virtualenv.py | ${PYTHON} - $VENV_DIR
  cd $VENV_DIR
  . bin/activate
  # set up mozbase
  git clone git://github.com/mozilla/mozbase.git
  cd mozbase
  python setup_development.py
fi

# update the marionette_client
cd $MARIONETTE_HOME
python setup.py develop
cd marionette

# pop off the python parameter
shift
cd scripts
python runemu.py $@

