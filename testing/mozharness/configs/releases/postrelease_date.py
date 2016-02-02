config = {
    "log_name": "bump_date",
    "version_files": [{"file": "browser/config/version_display.txt"}],
    "repo": {
        "repo": "https://hg.mozilla.org/projects/date",
        "revision": "default",
        "dest": "date",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/projects/date",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
}
