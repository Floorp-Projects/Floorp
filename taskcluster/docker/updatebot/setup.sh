#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

# Install the evolve extension
# Mercurial will complain that it can't find the evolve extension - this is
# because we don't have it yet, and we are asking mercurial to go install it
# so mercurial can use it.
hg clone https://repo.mercurial-scm.org/evolve/ "$HOME/.mozbuild/evolve"

# Copy the system known_hosts to the home directory so we have uniformity with Windows
# and the ssh command will find them in the same place.
cp /etc/ssh/ssh_known_hosts "$HOME/ssh_known_hosts"

# If poetry is not run as worker, then it won't work when run as user later.
cd /builds/worker/updatebot
poetry install --no-ansi
