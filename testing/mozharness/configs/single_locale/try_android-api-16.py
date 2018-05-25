import os

BRANCH = "try"
MOZILLA_DIR = BRANCH
EN_US_BINARY_URL = "http://archive.mozilla.org/pub/" \
                   "mobile/nightly/latest-mozilla-central-android-api-16/en-US"

config = {
    "branch": "try",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "is_automation": True,
    "locales_file": "%s/mobile/locales/l10n-changesets.json" % MOZILLA_DIR,
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "nightly_build": True,
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/try",
        "revision": "%(revision)s",
        "dest": "try",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
        "clone_by_revision": True,
        "clone_with_purge": True,
    }],
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': "/builds/hg-shared",

    "l10n_dir": "l10n-central",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        "MOZ_UPDATE_CHANNEL": "try", # XXX Invalid
    },
    "upload_branch": "%s-android-api-16" % BRANCH,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "platform": "android", # XXX Validate

    # Balrog
    "build_target": "Android_arm-eabi-gcc3",
}
