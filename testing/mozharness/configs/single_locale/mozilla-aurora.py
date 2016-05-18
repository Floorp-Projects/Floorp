config = {
    "nightly_build": True,
    "branch": "mozilla-aurora",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora/",
    "update_channel": "aurora",
    "latest_mar_dir": '/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora-l10n',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-aurora",

    # mar
    "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora/mar-tools/%(platform)s",

    # repositories
    "mozilla_dir": "mozilla-aurora",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/releases/mozilla-aurora",
        "revision": "default",
        "dest": "mozilla-aurora",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "revision": "RELEASE_AUTOMATION"
    }],
    # purge options
    'is_automation': True,
}
