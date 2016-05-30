config = {
    "nightly_build": True,
    "branch": "mozilla-esr45",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-esr45/",
    "update_channel": "esr",
    "latest_mar_dir": '/pub/mozilla.org/firefox/nightly/latest-mozilla-esr45-l10n',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-release",

    # repositories
    "mozilla_dir": "mozilla-esr45",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/releases/mozilla-esr45",
        "revision": "default",
        "dest": "mozilla-esr45",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "revision": "RELEASE_AUTOMATION"
    }],
    # purge options
    'purge_minsize': 12,
    'is_automation': True,
    'default_actions': [
        "clobber",
        "pull",
        "clone-locales",
        "list-locales",
        "setup",
        "repack",
        "taskcluster-upload",
        "summary",
    ],
}
