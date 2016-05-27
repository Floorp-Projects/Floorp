config = {
    "log_name": "bump_esr45",
    "version_files": [
        {"file": "browser/config/version.txt"},
        {"file": "browser/config/version_display.txt"},
        {"file": "config/milestone.txt"},
    ],
    "repo": {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr45",
        "branch": "default",
        "dest": "mozilla-esr45",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/releases/mozilla-esr45",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
}
