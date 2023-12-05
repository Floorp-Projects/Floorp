# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "appName": "Firefox",
    "log_name": "partner_repack",
    "repack_manifests_url": "git@github.com:mozilla-partners/repack-manifests.git",
    "repo_file": "https://raw.githubusercontent.com/mozilla-releng/git-repo/main/repo",
    "secret_files": [
        {
            "filename": "/builds/partner-github-ssh",
            "secret_name": "project/releng/gecko/build/level-%(scm-level)s/partner-github-ssh",
            "min_scm_level": 1,
            "mode": 0o600,
        },
    ],
    "ssh_key": "/builds/partner-github-ssh",
}
