import os
BRANCH = "mozilla-release"
MOZ_UPDATE_CHANNEL = "release"
MOZILLA_DIR = BRANCH
OBJDIR = "obj-firefox"
EN_US_BINARY_URL = None
HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    "branch": BRANCH,
    "log_name": "single_locale",
    "objdir": OBJDIR,
    "is_automation": True,
    "locales_file": "%s/mobile/locales/l10n-changesets.json" % MOZILLA_DIR,
    "locales_dir": "mobile/android/locales",
    "locales_platform": "android-api-16",
    "ignore_locales": ["en-US"],
    "balrog_credentials_file": "oauth.txt",
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "platform": "android",
    "is_release_or_beta": True,
    "build_target": "Android_arm-eabi-gcc3",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "repos": [{
        "repo": "https://hg.mozilla.org/releases/mozilla-release",
        "branch": "default",
        "dest": MOZILLA_DIR,
    }, {
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools"
    }],
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': HG_SHARE_BASE_DIR,
    "l10n_dir": "l10n-central",

    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        "MOZ_OBJDIR": OBJDIR,
        "MOZ_UPDATE_CHANNEL": MOZ_UPDATE_CHANNEL,
    },
    "upload_branch": "%s-android-api-16" % BRANCH,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "key_alias": "release",
}
