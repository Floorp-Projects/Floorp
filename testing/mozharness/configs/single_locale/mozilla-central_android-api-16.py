import os

BRANCH = "mozilla-central"
MOZ_UPDATE_CHANNEL = "nightly"
MOZILLA_DIR = BRANCH
OBJDIR = "obj-firefox"
EN_US_BINARY_URL = "http://archive.mozilla.org/pub/mobile/nightly/latest-%s-android-api-16/en-US" % BRANCH
HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    "branch": BRANCH,
    "log_name": "single_locale",
    "objdir": OBJDIR,
    "is_automation": True,
    "locales_file": "%s/mobile/locales/l10n-changesets.json" % MOZILLA_DIR,
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    "nightly_build": True,
    'balrog_credentials_file': 'oauth.txt',
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "%(revision)s",
        "dest": MOZILLA_DIR,
    }],
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': HG_SHARE_BASE_DIR,

    "l10n_dir": "l10n-central",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_OBJDIR": OBJDIR,
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        "MOZ_UPDATE_CHANNEL": MOZ_UPDATE_CHANNEL,
    },
    "upload_branch": "%s-android-api-16" % BRANCH,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "platform": "android",

    # Balrog
    "build_target": "Android_arm-eabi-gcc3",
}
