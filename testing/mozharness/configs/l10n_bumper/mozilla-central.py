import sys

MULTI_REPO = "mozilla-central"
EXES = {}
if sys.platform.startswith("linux"):
    EXES = {
        # Get around the https warnings
        "hg": ["/usr/local/bin/hg", "--config", "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt"],
        "hgtool.py": ["/usr/local/bin/hgtool.py"],
    }

config = {
    "log_name": "l10n_bumper",
    "exes": EXES,

    "gecko_pull_url": "https://hg.mozilla.org/{}".format(MULTI_REPO),
    "gecko_push_url": "ssh://hg.mozilla.org/{}".format(MULTI_REPO),

    "hg_user": "L10n Bumper Bot <release+l10nbumper@mozilla.com>",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ssh_user": "ffxbld",

    "vcs_share_base": "/builds/hg-shared",
    "version_path": "browser/config/version.txt",

    "bump_configs": [{
        "path": "mobile/locales/l10n-changesets.json",
        "format": "json",
        "name": "Fennec l10n changesets",
        "platform_configs": [{
            "platforms": ["android-api-16", "android"],
            "path": "mobile/android/locales/all-locales"
        }, {
            "platforms": ["android-multilocale"],
            "path": "mobile/android/locales/maemo-locales"
        }],
    }, {
        "path": "browser/locales/l10n-changesets.json",
        "format": "json",
        "name": "Firefox l10n changesets",
        "ignore_config": {
            "ja": ["macosx64", "macosx64-devedition"],
            "ja-JP-mac": [
                "linux", "linux-devedition", "linux64", "linux64-devedition",
                "win32", "win32-devedition", "win64", "win64-devedition",
            ],
        },
        "platform_configs": [{
            "platforms": [
                "linux", "linux-devedition", "linux64", "linux64-devedition",
                "macosx64", "macosx64-devedition", "win32", "win32-devedition",
                "win64", "win64-devedition",
            ],
            "path": "browser/locales/all-locales",
        }],
    }],
}
