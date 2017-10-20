config = {
    "branch": "jamun",
    "nightly_build": True,
    "update_channel": "release",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # repositories
    # staging release uses jamun
    "mozilla_dir": "jamun",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/projects/jamun",
        "branch": "%(revision)s",
        "dest": "jamun",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
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
