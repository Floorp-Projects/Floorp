# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    # maple is used for staging mozilla-beta
    "log_name": "bump_maple",
    "version_files": [{"file": "browser/config/version_display.txt"}],
    "repo": {
        # maple is used for staging mozilla-beta
        "repo": "https://hg.mozilla.org/projects/maple",
        "branch": "default",
        "dest": "maple",
        "vcs": "hg",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    },
    # maple is used for staging mozilla-beta
    "push_dest": "ssh://hg.mozilla.org/projects/maple",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ship_it_root": "https://ship-it-dev.allizom.org",
    "ship_it_username": "ship_it-stage-ffxbld",
}
