config = {
    "log_name": "bump_beta",
    "version_files": [{"file": "browser/config/version_display.txt"}],
    "repo": {
        "repo": "https://hg.mozilla.org/releases/mozilla-beta",
        "branch": "default",
        "dest": "mozilla-beta",
        "vcs": "hg",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    },
    "vcs_share_base": "/builds/hg-shared",
    "push_dest": "ssh://hg.mozilla.org/releases/mozilla-beta",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "ship_it_root": "https://ship-it.mozilla.org",
    "ship_it_username":  "ffxbld",
}
