# TODO Rename this file to bump_esr.py once esr60 is gone

import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
config = {
    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    "log_name": "bump_esr",
    "version_files": [
        {"file": "browser/config/version.txt", "suffix": ""},
        {"file": "browser/config/version_display.txt", "suffix": ""},
        {"file": "config/milestone.txt", "suffix": ""},

        {"file": "mobile/android/config/version-files/nightly/version.txt", "suffix": ""},
        {"file": "mobile/android/config/version-files/nightly/version_display.txt", "suffix": ""},

        {"file": "mobile/android/config/version-files/beta/version.txt", "suffix": ""},
        {"file": "mobile/android/config/version-files/beta/version_display.txt", "suffix": ""},

        {"file": "mobile/android/config/version-files/release/version.txt", "suffix": ""},
        {"file": "mobile/android/config/version-files/release/version_display.txt", "suffix": ""},
    ],
    "to_repo_url": "https://hg.mozilla.org/releases/mozilla-esr68",

    "migration_behavior": "bump_second_digit",
    "require_remove_locales": False,
    "requires_head_merge": False,
    "default_actions": [
        "clean-repos",
        "pull",
        "set_push_to_ssh",
        "bump_second_digit"
    ],
}
