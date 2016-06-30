#!/usr/bin/env python

HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    # mozharness script options
    "vcs_share_base": HG_SHARE_BASE_DIR,
    "default_vcs": "tc-vcs",
    "default_actions": [
        'checkout-sources',
        'get-blobs',
        'build',
        'prep-upload'
    ],
    "upload": {
        "default": {
            "upload_dep_target_exclusions": []
        }
    },
    "env": {
        "GAIA_OPTIMIZE": "1",
        "WGET_OPTS": "-c -q",
        "B2G_PATH": "%(work_dir)s",
        "BOWER_FLAGS": "--allow-root",
        "WGET_OPTS": "-c -q",
        "HG_SHARE_BASE_DIR": HG_SHARE_BASE_DIR,
        "B2G_ANDROID_NDK_PATH": "%(b2g_repo)s/android-ndk",
        "ANDROIDFS_DIR": "%(b2g_repo)s/backup-%(b2g_target)s",
    },
    "is_automation": True,
    "repo_remote_mappings": {
        'https://android.googlesource.com/': 'https://git.mozilla.org/external/aosp',
        'git://codeaurora.org/': 'https://git.mozilla.org/external/caf',
        'https://git.mozilla.org/b2g': 'https://git.mozilla.org/b2g',
        'git://github.com/mozilla-b2g/': 'https://git.mozilla.org/b2g',
        'git://github.com/mozilla/': 'https://git.mozilla.org/b2g',
        'https://git.mozilla.org/releases': 'https://git.mozilla.org/releases',
        'http://android.git.linaro.org/git-ro/': 'https://git.mozilla.org/external/linaro',
        'git://github.com/apitrace/': 'https://git.mozilla.org/external/apitrace',
    },
    "download_tooltool": True,
    "tooltool_servers": ['http://relengapi/tooltool/'],
}
