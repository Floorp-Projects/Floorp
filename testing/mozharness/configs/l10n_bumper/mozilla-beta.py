MULTI_REPO = "releases/mozilla-beta"
config = {
    "log_name": "l10n_bumper",

    "exes": {
        # Get around the https warnings
        "hg": ["/usr/local/bin/hg", "--config", "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt"],
        "hgtool.py": ["/usr/local/bin/hgtool.py"],
    },

#    "gecko_pull_url": "https://hg.mozilla.org/{}".format(MULTI_REPO),
#    "gecko_push_url": "ssh://hg.mozilla.org/{}".format(MULTI_REPO),
    "gecko_pull_url": "https://hg.mozilla.org/projects/jamun",
    "gecko_push_url": "ssh://hg.mozilla.org/projects/jamun",

    "hg_user": "L10n Bumper Bot <release+l10nbumper@mozilla.com>",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ssh_user": "ffxbld",

    "vcs_share_base": "/builds/hg-shared",
    "version_path": "browser/config/version.txt",

    "bump_configs": [{
        "path": "mobile/locales/l10n-changesets.json",
        "format": "json",
        "name": "Fennec l10n changesets",
        "url": "https://l10n.mozilla.org/shipping/json-changesets?av=fennec%(MAJOR_VERSION)s&platforms=android&multi_android-multilocale_repo={}&multi_android-multilocale_rev=default&multi_android-multilocale_path=mobile/android/locales/maemo-locales".format(MULTI_REPO),
    }],
}
