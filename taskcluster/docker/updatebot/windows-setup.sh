#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

. ./taskcluster/docker/updatebot/updatebot-version.sh # Get UPDATEBOT_REVISION

HOME=$(python3 -c "import os;print(os.path.expanduser('~'))")
export HOME
GECKO_PATH="$PWD"
UPDATEBOT_PATH="$MOZ_FETCHES_DIR/updatebot"

# MOZ_FETCHES_DIR is in Z:/ format.  When we update the PATH we need to use
#   /z/ format.  Fortunately, we can translate them like so:
cd "$MOZ_FETCHES_DIR"
MOZ_FETCHES_PATH="$PWD"

#########################################################
# Install dependencies

# Git is at /c/Program Files/Git/cmd/git.exe
#  It's in PATH for this script (confusingly) but not in PATH so we need to add it
which git
export PATH="/c/Program Files/Git/cmd:$PATH"

# php & arcanist
if [ -n "$TOOLTOOL_MANIFEST" ]; then
 . "$GECKO_PATH/taskcluster/scripts/misc/tooltool-download.sh"
fi

cp "$MOZ_FETCHES_DIR/vcruntime140.dll" "$MOZ_FETCHES_DIR/php-win"
cp "$GECKO_PATH/taskcluster/docker/updatebot/windows-php.ini" "$MOZ_FETCHES_DIR/php-win/php.ini"

cd "$MOZ_FETCHES_DIR/arcanist.git"
patch -p1 < "$GECKO_PATH/taskcluster/docker/updatebot/arcanist_windows_stream.patch"
cd "$MOZ_FETCHES_DIR"

export PATH="$MOZ_FETCHES_PATH/php-win:$PATH"
export PATH="$MOZ_FETCHES_PATH/arcanist.git/bin:$PATH"



ls "$MOZ_FETCHES_PATH/arcanist.git/bin"

# get Updatebot
cd "$MOZ_FETCHES_DIR"
git clone https://github.com/mozilla-services/updatebot.git
cd updatebot
git checkout "$UPDATEBOT_REVISION"

# base python needs
python3 -m pip install --no-warn-script-location --user -U pip
python3 -m pip install --no-warn-script-location --user poetry wheel requests setuptools

# updatebot dependencies
cd "$UPDATEBOT_PATH"
python3 -m poetry install

# taskcluster secrets and writing out localconfig
cd "$GECKO_PATH"
python3 ./taskcluster/docker/updatebot/run.py "$GECKO_PATH" "$UPDATEBOT_PATH" "$MOZ_FETCHES_PATH"

# mercurial configuration
cp "$GECKO_PATH/taskcluster/docker/updatebot/hgrc" "$HOME/.hgrc"
# Windows is not happy with $HOME in the hgrc so we need to do a hack to replace it
#   with the actual value
( echo "cat <<EOF" ; cat "$HOME/.hgrc" ) | sh > tmp
mv tmp "$HOME/.hgrc"

# ssh known hosts
cp "$GECKO_PATH/taskcluster/docker/push-to-try/known_hosts" "$HOME/ssh_known_hosts"

#########################################################
# Run it
export PYTHONIOENCODING=utf8
export PYTHONUNBUFFERED=1

cd "$UPDATEBOT_PATH"
python3 -m poetry run python3 ./automation.py

#########################################################
echo "Killing SQL Proxy"
taskkill -f -im cloud_sql_proxy.exe || true
