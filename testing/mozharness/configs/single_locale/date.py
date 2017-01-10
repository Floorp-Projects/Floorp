config = {
    "nightly_build": True,
    "branch": "date",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly-date",
    "latest_mar_dir": '/pub/firefox/nightly/latest-date-l10n',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s",

    # repositories
    "mozilla_dir": "date",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/projects/date",
        "revision": "%(revision)s",
        "dest": "date",
    }],
    # purge options
    'is_automation': True,
}
