config = {
    "nightly_build": True,
    "branch": "mozilla-esr52",
    "en_us_binary_url": "https://archive.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-esr52/",
    "update_channel": "esr",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-release",

    # repositories
    "mozilla_dir": "mozilla-esr52",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/releases/mozilla-esr52",
        "revision": "%(revision)s",
        "dest": "mozilla-esr52",
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
