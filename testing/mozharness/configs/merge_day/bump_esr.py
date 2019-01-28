import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
config = {
    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    "log_name": "bump_esr",
    "version_files": [
        {"file": "browser/config/version.txt", "suffix": ""},
        {"file": "browser/config/version_display.txt", "suffix": ""},
        {"file": "config/milestone.txt", "suffix": ""},
    ],
    "to_repo_url": "https://hg.mozilla.org/releases/mozilla-esr60",

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
