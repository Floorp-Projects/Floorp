NEW_ESR_REPO = "ssh://hg.mozilla.org/releases/mozilla-esr45"
OLD_ESR_REPO = "https://hg.mozilla.org/releases/mozilla-esr38"
OLD_ESR_CHANGESET = "16351963d75c"

config = {
    "log_name": "relese_to_esr",
    "version_files": [
        "browser/config/version.txt",
        "browser/config/version_display.txt",
        "config/milestone.txt",
        "b2g/confvars.sh",
    ],
    "replacements": [
        # File, from, to
        ("browser/confvars.sh",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-release",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-esr"),
        ("browser/confvars.sh",
         "MAR_CHANNEL_ID=firefox-mozilla-release",
         "MAR_CHANNEL_ID=firefox-mozilla-esr"),
    ],
    # Disallow sharing, since we want pristine .hg directories.
    # "vcs_share_base": None,
    # "hg_share_base": None,
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_revision": "default",
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-release",
    "to_repo_url": NEW_ESR_REPO,

    "base_tag": "FIREFOX_ESR_%(major_version)s_BASE",
    "end_tag": "FIREFOX_ESR_%(major_version)s_END",

    "migration_behavior": "release_to_esr",
    "require_remove_locales": False,
    "transplant_patches": [
        {"repo": OLD_ESR_REPO,
         "changeset": OLD_ESR_CHANGESET},
    ],
    "requires_head_merge": False,
    "pull_all_branches": True,
}
