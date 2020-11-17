# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

MULTI_REPO = "releases/mozilla-esr68"
EXES = {}
if sys.platform.startswith("linux"):
    EXES = {
        # Get around the https warnings
        "hg": [
            "/usr/local/bin/hg",
            "--config",
            "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt",
        ],
        "hgtool.py": ["/usr/local/bin/hgtool.py"],
    }

config = {
    "log_name": "l10n_bumper",
    "log_type": "multi",
    "exes": EXES,
    "gecko_pull_url": "https://hg.mozilla.org/{}".format(MULTI_REPO),
    "gecko_push_url": "ssh://hg.mozilla.org/{}".format(MULTI_REPO),
    "hg_user": "L10n Bumper Bot <release+l10nbumper@mozilla.com>",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ssh_user": "ffxbld",
    "vcs_share_base": "/builds/hg-shared",
    "version_path": "mobile/android/config/version-files/release/version.txt",
    "status_path": ".l10n_bumper_status",
    "bump_configs": [
        {
            "path": "mobile/locales/l10n-changesets.json",
            "format": "json",
            "name": "Fennec l10n changesets",
            "revision_url": "https://l10n.mozilla.org/shipping/l10n-changesets?av=fennec%(COMBINED_MAJOR_VERSION)s",
            "platform_configs": [
                {
                    "platforms": ["android-multilocale"],
                    "path": "mobile/android/locales/maemo-locales",
                }
            ],
        }
    ],
}
