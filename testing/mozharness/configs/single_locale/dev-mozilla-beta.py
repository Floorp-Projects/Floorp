config = {
    "branch": "date",
    "nightly_build": True,
    "update_channel": "beta-dev",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-beta",

    # mar
    "latest_mar_dir": "fake_kill_me",

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
        "branch": "default",
        "dest": "date",
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
}
