# Use this script in conjunction with aurora_to_beta.py.
# mozharness/scripts/merge_day/gecko_migration.py -c \
#   mozharness/configs/merge_day/aurora_to_beta.py -c
#   mozharness/configs/merge_day/staging_beta_migration.py ...

config = {
    "log_name": "staging_beta",

    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_revision": "default",
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-aurora",
    "to_repo_url": "ssh://hg.mozilla.org/users/stage-ffxbld/mozilla-beta",

    "base_tag": "FIREFOX_BETA_%(major_version)s_BASE",
    "end_tag": "FIREFOX_BETA_%(major_version)s_END",

    "migration_behavior": "aurora_to_beta",
}
