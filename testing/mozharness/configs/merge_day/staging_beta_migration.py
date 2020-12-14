# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Use this script in conjunction with aurora_to_beta.py.
# mozharness/scripts/merge_day/gecko_migration.py -c \
#   mozharness/configs/merge_day/aurora_to_beta.py -c
#   mozharness/configs/merge_day/staging_beta_migration.py ...
from __future__ import absolute_import
import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")

config = {
    "log_name": "staging_beta",
    "vcs_share_base": os.path.join(ABS_WORK_DIR, "hg-shared"),
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-aurora",
    "to_repo_url": "ssh://hg.mozilla.org/users/stage-ffxbld/mozilla-beta",
    "base_tag": "FIREFOX_BETA_%(major_version)s_BASE",
    "end_tag": "FIREFOX_BETA_%(major_version)s_END",
    "migration_behavior": "aurora_to_beta",
}
