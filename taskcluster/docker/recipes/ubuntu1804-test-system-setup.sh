#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

cd /setup

# Install tooltool, mercurial and node now that dependencies are in place.
. /setup/common.sh
. /setup/install-mercurial.sh
. /setup/install-node.sh

# Upgrade pip and install virtualenv to specified versions.
pip install --upgrade pip==19.2.3
hash -r
pip install virtualenv==15.2.0

pip install zstandard==0.13.0
pip3 install zstandard==0.13.0

pip install psutil==5.7.0
pip3 install psutil==5.7.0

# Cleanup
cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
rm -f "$0"
