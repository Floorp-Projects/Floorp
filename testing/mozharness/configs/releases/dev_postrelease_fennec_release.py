config = {
    "log_name": "bump_release_dev",
    "version_files": [
        {"file": "browser/config/version.txt"},
        {"file": "browser/config/version_display.txt"},
        {"file": "config/milestone.txt"},
    ],
    "repo": {
        # jamun is used for staging mozilla-release
        "repo": "https://hg.mozilla.org/projects/jamun",
        "branch": "default",
        "dest": "jamun",
        "vcs": "hg",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    },
    "push_dest": "ssh://hg.mozilla.org/projects/jamun",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ship_it_root": "https://ship-it-dev.allizom.org",
    "ship_it_username":  "ship_it-stage-ffxbld",
}
