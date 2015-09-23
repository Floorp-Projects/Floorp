config = {
    "log_name": "aurora_to_beta",
    "version_files": [
        "browser/config/version.txt",
        "browser/config/version_display.txt",
        "config/milestone.txt",
        "b2g/confvars.sh",
    ],
    "replacements": [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=mobile/android/branding/aurora",
        "ac_add_options --with-branding=mobile/android/branding/beta")
        for d in ["mobile/android/config/mozconfigs/android-api-11/",
                  "mobile/android/config/mozconfigs/android-api-9-10-constrained/",
                  "mobile/android/config/mozconfigs/android-x86/"]
        for f in ["debug", "nightly", "l10n-nightly"]
    ] + [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=browser/branding/aurora",
        "ac_add_options --with-branding=browser/branding/nightly")
        for d in ["browser/config/mozconfigs/linux32",
                  "browser/config/mozconfigs/linux64",
                  "browser/config/mozconfigs/win32",
                  "browser/config/mozconfigs/win64",
                  "browser/config/mozconfigs/macosx64"]
        for f in ["debug", "nightly"]
    ] + [
        # File, from, to
        (f, "ac_add_options --with-branding=browser/branding/aurora", "")
        for f in ["browser/config/mozconfigs/linux32/l10n-mozconfig",
                  "browser/config/mozconfigs/linux64/l10n-mozconfig",
                  "browser/config/mozconfigs/win32/l10n-mozconfig",
                  "browser/config/mozconfigs/win64/l10n-mozconfig",
                  "browser/config/mozconfigs/macosx-universal/l10n-mozconfig",
                  "browser/config/mozconfigs/macosx64/l10n-mozconfig"]
    ] + [
        ("browser/config/mozconfigs/macosx-universal/nightly",
         "ac_add_options --with-branding=browser/branding/aurora",
         "ac_add_options --with-branding=browser/branding/nightly"),
        ("browser/confvars.sh",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-aurora",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-beta,firefox-mozilla-release"),
        ("browser/confvars.sh",
         "MAR_CHANNEL_ID=firefox-mozilla-aurora",
         "MAR_CHANNEL_ID=firefox-mozilla-beta"),
        ("browser/config/mozconfigs/whitelist",
         "ac_add_options --with-branding=browser/branding/aurora",
         "ac_add_options --with-branding=browser/branding/nightly"),
    ],

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
