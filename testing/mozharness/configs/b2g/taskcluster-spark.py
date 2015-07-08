#!/usr/bin/env python
config = {
    "default_vcs": "tc-vcs",
    "default_actions": [
        'checkout-sources',
        'build',
        'build-symbols',
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
        "LIGHTSABER": "1",
        "B2G_PATH": "%(work_dir)s",
        "BOWER_FLAGS": "--allow-root",
        "GAIA_DISTRIBUTION_DIR": "%(work_dir)s/gaia/distros/spark",
        "WGET_OPTS": "-c -q",
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
}
