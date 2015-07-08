#!/usr/bin/env python
import os.path
config = {
    "default_vcs": "tc-vcs",
    "default_actions": [
        'checkout-sources',
        'build',
        'build-symbols',
        'make-updates',
        'prep-upload',
        'submit-to-balrog'
    ],
    "balrog_credentials_file": "balrog_credentials",
    "nightly_build": True,
    "upload": {
        "default": {
            "upload_dep_target_exclusions": []
        }
    },
    "upload": {
        "default": {
            "ssh_key": os.path.expanduser("~/.ssh/b2g-rsa"),
            "ssh_user": "",
            "upload_remote_host": "",
            "upload_remote_nightly_path": "/srv/ftp/pub/mozilla.org/b2g/nightly/%(branch)s-%(target)s/latest",
            "upload_remote_path": "/srv/ftp/pub/mozilla.org/b2g/%(target)s/%(branch)s-%(target)s/%(buildid)s",
            "upload_remote_symlink": "/srv/ftp/pub/mozilla.org/b2g/%(target)s/%(branch)s-%(target)s/latest",
        },
        "public": {
            "ssh_key": os.path.expanduser("~/.ssh/b2g-rsa"),
            "ssh_user": "",
            "upload_remote_host": "",
            "post_upload_cmd": "post_upload.py --tinderbox-builds-dir %(branch)s-%(target)s -p b2g -i %(buildid)s --revision %(revision)s --release-to-tinderbox-dated-builds",
            "post_upload_nightly_cmd": "post_upload.py --tinderbox-builds-dir %(branch)s-%(target)s -b %(branch)s-%(target)s -p b2g -i %(buildid)s --revision %(revision)s --release-to-tinderbox-dated-builds --release-to-latest --release-to-dated",
        },
    },
    "env": {
        "GAIA_OPTIMIZE": "1",
        "WGET_OPTS": "-c -q"
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
