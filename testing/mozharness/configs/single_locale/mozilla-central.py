config = {
    "nightly_build": True,
    "branch": "mozilla-central",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly",
    "latest_mar_dir": '/pub/firefox/nightly/latest-mozilla-central-l10n',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s",

    # repositories
    "mozilla_dir": "mozilla-central",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "%(revision)s",
        "dest": "mozilla-central",
    }],
    # purge options
    'is_automation': True,
}
