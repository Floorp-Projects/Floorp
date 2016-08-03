config = {
    "log_name": "bump_esr38",
    "version_files": [
        {"file": "browser/config/version.txt"},
        # TODO: enable this for esr45
        # {"file": "browser/config/version_display.txt"},
        {"file": "config/milestone.txt"},
    ],
    "repo": {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr38",
        "branch": "default",
        "dest": "mozilla-esr38",
        "vcs": "hg",
    },
    "vcs_share_base": "/builds/hg-shared",
    "push_dest": "ssh://hg.mozilla.org/releases/mozilla-esr38",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
}
