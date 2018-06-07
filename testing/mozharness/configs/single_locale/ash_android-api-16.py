import os

BRANCH = "ash"
MOZ_UPDATE_CHANNEL = "nightly"
MOZILLA_DIR = BRANCH
OBJDIR = "obj-firefox"
HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    "branch": BRANCH,
    "log_name": "single_locale",
    "objdir": OBJDIR,
    "is_automation": True,
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    "nightly_build": True,
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': HG_SHARE_BASE_DIR,

    "l10n_dir": "l10n-central",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_OBJDIR": OBJDIR,
        "EN_US_BINARY_URL": os.environ['EN_US_BINARY_URL'],
        "MOZ_UPDATE_CHANNEL": MOZ_UPDATE_CHANNEL,
    },

    # Balrog
    "build_target": "Android_arm-eabi-gcc3",
}
