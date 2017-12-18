config = {
    "branch": "date",
    "nightly_build": True,
    "update_channel": "aurora",  # devedition uses aurora based branding

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # repositories
    # staging beta dev releases use date repo for now
    "mozilla_dir": "date",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/projects/date",
        "branch": "%(revision)s",
        "dest": "date",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    }],
    # purge options
    'is_automation': True,
    'purge_minsize': 12,
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

    "update_channel": "aurora",
}
