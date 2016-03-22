config = {
    "log_name": "bump_beta",
    "version_files": [{"file": "browser/config/version_display.txt"}],
    "repo": {
        "repo": "https://hg.mozilla.org/releases/mozilla-beta",
        "revision": "default",
        "dest": "mozilla-beta",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/releases/mozilla-beta",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
}
