#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

PYTHON=$1

if [ -z "${PYTHON}" ]
then
    echo "No python found"
    exit 1
fi

VIRTUAL_ENV_VERSION="49f40128a9ca3824ebf253eca408596e135cf893"

# Check if environment exists, if not, create a virtualenv:
if [ -d "marionette_auto_venv" ]
then
  cd marionette_auto_venv
  . bin/activate
else
  curl https://raw.github.com/pypa/virtualenv/${VIRTUAL_ENV_VERSION}/virtualenv.py | ${PYTHON} - marionette_auto_venv 
  cd marionette_auto_venv
  . bin/activate

  # set up mozbase
  git clone git://github.com/mozilla/mozbase.git
  cd mozbase
  python setup_development.py
  cd ..

  # set up mozautolog
  hg clone http://hg.mozilla.org/automation/mozautolog/
  cd mozautolog
  python setup.py develop
  cd ..

  # set up gitpython
  easy_install http://pypi.python.org/packages/source/G/GitPython/GitPython-0.3.2.RC1.tar.gz
fi

cd ../..
# update the marionette_client
python setup.py develop
cd marionette

#pop off the python parameter
shift
python runtests.py $@
