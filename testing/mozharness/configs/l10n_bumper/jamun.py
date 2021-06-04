# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys

MULTI_REPO = "projects/jamun"
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
    "version_path": "browser/config/version.txt",
    "status_path": ".l10n_bumper_status",
    "bump_configs": [
        {
            "path": "mobile/locales/l10n-changesets.json",
            "format": "json",
            "name": "Fennec l10n changesets",
            "revision_url": "https://l10n.mozilla.org/shipping/l10n-changesets?av=fennec%(MAJOR_VERSION)s",
            "platform_configs": [
                {
                    "platforms": ["android-arm", "android"],
                    "path": "mobile/android/locales/all-locales",
                },
                {
                    "platforms": ["android-multilocale"],
                    "path": "mobile/android/locales/maemo-locales",
                },
            ],
        },
        {
            "path": "browser/locales/l10n-changesets.json",
            "format": "json",
            "name": "Firefox l10n changesets",
            "revision_url": "https://l10n.mozilla.org/shipping/l10n-changesets?av=fx%(MAJOR_VERSION)s",
            "ignore_config": {
                "ja": ["macosx64"],
                "ja-JP-mac": ["linux", "linux64", "win32", "win64"],
            },
            "platform_configs": [
                {
                    "platforms": ["linux64", "linux", "macosx64", "win32", "win64"],
                    "path": "browser/locales/shipped-locales",
                    "format": "shipped-locales",
                }
            ],
        },
        {
            "path": "browser/locales/central-changesets.json",
            "format": "json",
            "name": "Firefox l10n changesets",
            "ignore_config": {
                "ja": ["macosx64"],
                "ja-JP-mac": ["linux", "linux64", "win32", "win64"],
            },
            "platform_configs": [
                {
                    "platforms": ["linux64", "linux", "macosx64", "win32", "win64"],
                    "path": "browser/locales/all-locales",
                }
            ],
        },
    ],
}
