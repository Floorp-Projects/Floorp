config = {
    "log_name": "aurora_to_beta",

    "branding_dirs": [
        "mobile/android/config/mozconfigs/android-api-11/",
        "mobile/android/config/mozconfigs/android-api-9-10-constrained/",
        "mobile/android/config/mozconfigs/android-x86/",
    ],
    "branding_files": ["debug", "l10n-nightly", "nightly"],

    # Disallow sharing, since we want pristine .hg directories.
    # "vcs_share_base": None,
    # "hg_share_base": None,
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_revision": "default",
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-aurora",
    "to_repo_url": "ssh://hg.mozilla.org/releases/mozilla-beta",

    "base_tag": "FIREFOX_BETA_%(major_version)s_BASE",
    "end_tag": "FIREFOX_BETA_%(major_version)s_END",

    "migration_behavior": "aurora_to_beta",

    "virtualenv_modules": [
        "requests==2.2.1",
    ],

    "post_merge_builders": [
        "mozilla-beta hg bundle",
    ],
    "post_merge_nightly_branches": [
        # No nightlies on mozilla-beta
    ],
}
