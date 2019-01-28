import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")

config = {
    "log_name": "bump_central",
    "version_files": [
        {"file": "browser/config/version.txt", "suffix": ""},
        {"file": "browser/config/version_display.txt", "suffix": "b1"},
        {"file": "config/milestone.txt", "suffix": ""},
    ],

    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    "to_repo_url": "https://hg.mozilla.org/mozilla-central",

    "end_tag": "FIREFOX_NIGHTLY_%(major_version)s_END",

    "virtualenv_modules": [
        "requests==2.8.1",
    ],

    "require_remove_locales": False,
    "requires_head_merge": False,

    "migration_behavior": "bump_and_tag_central", # like esr_bump.py, needed for key validation
    "default_actions": [
        "clean-repos",
        "pull",
        "set_push_to_ssh",
        "bump_and_tag_central"
    ],
}
