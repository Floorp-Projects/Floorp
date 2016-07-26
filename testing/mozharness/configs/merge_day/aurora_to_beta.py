import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")

config = {
    "log_name": "aurora_to_beta",
    "version_files": [
        {"file": "browser/config/version.txt", "suffix": ""},
        {"file": "browser/config/version_display.txt", "suffix": "b1"},
        {"file": "config/milestone.txt", "suffix": ""},
    ],
    "replacements": [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=mobile/android/branding/aurora",
        "ac_add_options --with-branding=mobile/android/branding/beta")
        for d in ["mobile/android/config/mozconfigs/android-api-15/",
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
        (f, "ac_add_options --with-branding=browser/branding/aurora",
         "ac_add_options --enable-official-branding")
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
    ] + [
        ("build/mozconfig.common",
         "MOZ_REQUIRE_SIGNING=${MOZ_REQUIRE_SIGNING-0}",
         "MOZ_REQUIRE_SIGNING=${MOZ_REQUIRE_SIGNING-1}"),
        ("build/mozconfig.common",
         "# Disable enforcing that add-ons are signed by the trusted root",
         "# Enable enforcing that add-ons are signed by the trusted root")
    ],

    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    # "hg_share_base": None,
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_branch": "default",
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-aurora",
    "to_repo_url": "ssh://hg.mozilla.org/releases/mozilla-beta",

    "base_tag": "FIREFOX_BETA_%(major_version)s_BASE",
    "end_tag": "FIREFOX_BETA_%(major_version)s_END",

    "migration_behavior": "aurora_to_beta",

    "virtualenv_modules": [
        "requests==2.8.1",
    ],

    "post_merge_builders": [],
    "post_merge_nightly_branches": [
        # No nightlies on mozilla-beta
    ],
}
