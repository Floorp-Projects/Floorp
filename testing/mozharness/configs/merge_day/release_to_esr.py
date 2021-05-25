# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
NEW_ESR_REPO = "https://hg.mozilla.org/releases/mozilla-esr68"

config = {
    "log_name": "relese_to_esr",
    "version_files": [
        {"file": "browser/config/version_display.txt", "suffix": "esr"},
    ],
    "replacements": [],
    "vcs_share_base": os.path.join(ABS_WORK_DIR, "hg-shared"),
    # Pull from ESR repo, since we have already branched it and have landed esr-specific patches on it
    # We will need to manually merge mozilla-release into before runnning this.
    "from_repo_url": NEW_ESR_REPO,
    "to_repo_url": NEW_ESR_REPO,
    "base_tag": "FIREFOX_ESR_%(major_version)s_BASE",
    "migration_behavior": "release_to_esr",
    "require_remove_locales": False,
    "requires_head_merge": False,
    "pull_all_branches": False,
}
