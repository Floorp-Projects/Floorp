config = {
    "log_name": "bump_esr52",
    "version_files": [
        {"file": "browser/config/version.txt"},
        {"file": "browser/config/version_display.txt"},
        {"file": "config/milestone.txt"},
    ],
    "repo": {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr52",
        "branch": "default",
        "dest": "mozilla-esr52",
        "vcs": "hg",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    },
    "vcs_share_base": "/builds/hg-shared",
    "push_dest": "ssh://hg.mozilla.org/releases/mozilla-esr52",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ship_it_root": "https://ship-it.mozilla.org",
    "ship_it_username":  "ship_it-ffxbld",
}
