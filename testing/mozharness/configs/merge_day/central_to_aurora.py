config = {
    "log_name": "central_to_aurora",

    "replacements": [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=mobile/android/branding/nightly",
        "ac_add_options --with-branding=mobile/android/branding/aurora")
        for d in ["mobile/android/config/mozconfigs/android-api-11/",
                  "mobile/android/config/mozconfigs/android-api-9-10-constrained/",
                  "mobile/android/config/mozconfigs/android-x86/"]
        for f in ["debug", "nightly", "l10n-nightly"]
    ] + [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=browser/branding/nightly",
        "ac_add_options --with-branding=browser/branding/aurora")
        for d in ["browser/config/mozconfigs/linux32",
                  "browser/config/mozconfigs/linux64",
                  "browser/config/mozconfigs/win32",
                  "browser/config/mozconfigs/win64",
                  "browser/config/mozconfigs/macosx64"]
        for f in ["debug", "nightly"]
    ] + [
        ("browser/config/mozconfigs/macosx-universal/nightly",
        "ac_add_options --with-branding=browser/branding/nightly",
        "ac_add_options --with-branding=browser/branding/aurora")

    ] + [
        # File, from, to
        ("{}/l10n-nightly".format(d),
        "ac_add_options --with-l10n-base=../../l10n-central",
        "ac_add_options --with-l10n-base=..")
        for d in ["mobile/android/config/mozconfigs/android-api-11/",
                  "mobile/android/config/mozconfigs/android-api-9-10-constrained/",
                  "mobile/android/config/mozconfigs/android-x86/"]
    ] + [
        # File, from, to
        (f, "ac_add_options --enable-profiling", "") for f in
        ["mobile/android/config/mozconfigs/android-api-11/nightly",
         "mobile/android/config/mozconfigs/android-api-9-10-constrained/nightly",
         "mobile/android/config/mozconfigs/android-x86/nightly",
         "browser/config/mozconfigs/linux32/nightly",
         "browser/config/mozconfigs/linux64/nightly",
         "browser/config/mozconfigs/macosx-universal/nightly",
         "browser/config/mozconfigs/win32/nightly",
         "browser/config/mozconfigs/win64/nightly"]
    ] + [
        # File, from, to
        ("browser/confvars.sh",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-central",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-aurora"),
        ("browser/confvars.sh",
         "MAR_CHANNEL_ID=firefox-mozilla-central",
         "MAR_CHANNEL_ID=firefox-mozilla-aurora"),
    ],
    "locale_files": [
        "browser/locales/shipped-locales",
        "browser/locales/all-locales",
        "mobile/android/locales/maemo-locales",
        "mobile/android/locales/all-locales"
    ],

    # Disallow sharing, since we want pristine .hg directories.
    # "vcs_share_base": None,
    # "hg_share_base": None,
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_revision": "default",
    "from_repo_url": "ssh://hg.mozilla.org/mozilla-central",
    "to_repo_url": "ssh://hg.mozilla.org/releases/mozilla-aurora",

    "base_tag": "FIREFOX_AURORA_%(major_version)s_BASE",
    "end_tag": "FIREFOX_AURORA_%(major_version)s_END",

    "migration_behavior": "central_to_aurora",

    "balrog_rules_to_lock": [
        8,  # Fennec aurora channel
        10,  # Firefox aurora channel
        18,  # MetroFirefox aurora channel
        106,  # Fennec api-9 aurora channel
    ],
    "balrog_credentials_file": "oauth.txt",

    "virtualenv_modules": [
        "requests==2.2.1",
    ],

    "post_merge_builders": [
        "mozilla-aurora hg bundle",
        "mozilla-central hg bundle",
    ],
    "post_merge_nightly_branches": [
        "mozilla-central",
        "mozilla-aurora",
    ],
}
