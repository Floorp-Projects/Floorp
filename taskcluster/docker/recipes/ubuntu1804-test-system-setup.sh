#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

cd /setup

# Install tooltool and node now that dependencies are in place.
. /setup/common.sh
. /setup/install-node.sh

# Upgrade pip and install virtualenv to specified versions.
pip install --upgrade pip==19.2.3
hash -r
pip install virtualenv==15.2.0

pip3 install -r /setup/psutil_requirements.txt
pip install -r /setup/psutil_requirements.txt

# Cleanup
cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
rm -f "$0"
