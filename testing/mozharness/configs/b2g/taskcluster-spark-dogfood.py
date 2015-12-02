#!/usr/bin/env python
import os.path
config = {
    "default_vcs": "tc-vcs",
    "default_actions": [
        'checkout-sources',
        'build',
        'build-symbols',
        'make-updates',
        'prep-upload'
    ],
    "nightly_build": True,
    "env": {
        "GAIA_OPTIMIZE": "1",
        "B2G_UPDATER": "1",
        "LIGHTSABER": "1",
        "DOGFOOD": "1",
        "BOWER_FLAGS": "--allow-root",
        "B2G_PATH": "%(work_dir)s",
        "GAIA_DISTRIBUTION_DIR": "%(work_dir)s/gaia/distros/spark",
        "WGET_OPTS": "-c -q",
        "B2G_FOTA_FULLIMG_PARTS": "/boot:boot.img /system:system.img /recovery:recovery.img"
    },
    "update_types": [ "ota", "fota", "fota:fullimg" ],
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
}
