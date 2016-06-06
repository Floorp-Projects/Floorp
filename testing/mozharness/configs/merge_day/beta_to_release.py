import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")

config = {
    "log_name": "beta_to_release",
    "copy_files": [
        {
            "src": "browser/config/version.txt",
            "dst": "browser/config/version_display.txt",
        },
    ],
    "replacements": [
        # File, from, to
        ("{}/{}".format(d, f),
        "ac_add_options --with-branding=mobile/android/branding/beta",
        "ac_add_options --with-branding=mobile/android/branding/official")
        for d in ["mobile/android/config/mozconfigs/android-api-15/",
                  "mobile/android/config/mozconfigs/android-api-9-10-constrained/",
                  "mobile/android/config/mozconfigs/android-x86/"]
        for f in ["debug", "nightly", "l10n-nightly", "l10n-release", "release"]
    ] + [
        # File, from, to
        ("browser/confvars.sh",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-beta,firefox-mozilla-release",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-release"),
        ("browser/confvars.sh",
         "MAR_CHANNEL_ID=firefox-mozilla-beta",
         "MAR_CHANNEL_ID=firefox-mozilla-release"),
    ],

    "use_vcs_unique_share": True,
    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    # "hg_share_base": None,
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_branch": "default",
    "from_repo_url": "ssh://hg.mozilla.org/releases/mozilla-beta",
    "to_repo_url": "ssh://hg.mozilla.org/releases/mozilla-release",

    "base_tag": "FIREFOX_RELEASE_%(major_version)s_BASE",
    "end_tag": "FIREFOX_RELEASE_%(major_version)s_END",

    "migration_behavior": "beta_to_release",
    "require_remove_locales": False,
    "pull_all_branches": True,

    "virtualenv_modules": [
        "requests==2.8.1",
    ],

    "post_merge_builders": [],
    "post_merge_nightly_branches": [
        # No nightlies on mozilla-release
    ],
}
