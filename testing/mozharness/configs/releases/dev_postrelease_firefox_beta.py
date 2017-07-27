config = {
    # date is used for staging mozilla-beta
    "log_name": "bump_date",
    "version_files": [{"file": "browser/config/version_display.txt"}],
    "repo": {
        # date is used for staging mozilla-beta
        "repo": "https://hg.mozilla.org/projects/date",
        "branch": "default",
        "dest": "date",
        "vcs": "hg",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    },
    # date is used for staging mozilla-beta
    "push_dest": "ssh://hg.mozilla.org/projects/date",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ship_it_root": "https://ship-it-dev.allizom.org",
    "ship_it_username":  "ship_it-stage-ffxbld",
}
