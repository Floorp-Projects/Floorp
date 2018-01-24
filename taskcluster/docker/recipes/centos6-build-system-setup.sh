#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cd /setup || exit

# shellcheck source=taskcluster/docker/recipes/common.sh
. /setup/common.sh
# shellcheck source=taskcluster/docker/recipes/install-mercurial.sh
. /setup/install-mercurial.sh
# shellcheck source=taskcluster/docker/recipes/install-make.sh
. /setup/install-make.sh
# shellcheck source=taskcluster/docker/recipes/install-cmake.sh
. /setup/install-cmake.sh

if [ -f /setup/install-debug-symbols.sh ]; then
    # shellcheck disable=SC1091
    . /setup/install-debug-symbols.sh
fi

rm -rf /setup
