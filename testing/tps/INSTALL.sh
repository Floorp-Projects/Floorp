#!/bin/bash

# This scripts sets up a virutalenv and installs TPS into it.
# It's probably best to specify a path NOT inside the repo, otherwise
# all the virtualenv files will show up in e.g. hg status.

# get target directory
if [ ! -z "$1" ]
then
  TARGET=$1
else
  echo "Usage: INSTALL.sh /path/to/create/virtualenv [/path/to/python2.6]"
  exit 1
fi

# decide which python to use
if [ ! -z "$2" ]
then
    PYTHON=$2
else
    PYTHON=`which python`
fi
if [ -z "${PYTHON}" ]
then
    echo "No python found"
    exit 1
fi

CWD="`pwd`"

# create the destination directory
mkdir ${TARGET}

if [ "$?" -gt 0 ]
then
  exit 1
fi

if [ "${OS}" = "Windows_NT" ]
then
  BIN_NAME=Scripts/activate
else
  BIN_NAME=bin/activate
fi

# Create a virtualenv:
curl https://raw.github.com/jonallengriffin/virtualenv/msys/virtualenv.py | ${PYTHON} - ${TARGET}
cd ${TARGET}
. $BIN_NAME
if [ -z "${VIRTUAL_ENV}" ]
then
    echo "virtualenv wasn't installed correctly, aborting"
    exit 1
fi

# install TPS
cd ${CWD}
python setup.py install

if [ "$?" -gt 0 ]
then
  exit 1
fi

CONFIG="`find ${VIRTUAL_ENV} -name config.json.in`"
NEWCONFIG=${CONFIG:0:${#CONFIG}-3}

cd "../../services/sync/tests/tps"
TESTDIR="`pwd`"

cd "../../tps/extensions"
EXTDIR="`pwd`"

sed 's|__TESTDIR__|'"${TESTDIR}"'|' "${CONFIG}" | sed 's|__EXTENSIONDIR__|'"${EXTDIR}"'|' > "${NEWCONFIG}"
rm ${CONFIG}

echo
echo "***********************************************************************"
echo
echo "To run TPS, activate the virtualenv using:"
echo "  source ${TARGET}/${BIN_NAME}"
echo "then execute tps using:"
echo "  runtps --binary=/path/to/firefox"
echo
echo "See runtps --help for all options"
echo
echo "To change your TPS config, please edit the file: "
echo "${NEWCONFIG}"
echo
echo "***********************************************************************"
