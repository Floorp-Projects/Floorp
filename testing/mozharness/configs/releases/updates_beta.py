
config = {
    "log_name": "updates_date",
    "repo": {
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/build/tools",
    "shipped-locales-url": "https://hg.mozilla.org/releases/mozilla-beta/raw-file/{revision}/browser/locales/shipped-locales",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "archive_domain": "archive.mozilla.org",
    "archive_prefix": "https://archive.mozilla.org",
    "previous_archive_prefix": "https://archive.mozilla.org",
    "download_domain": "download.mozilla.org",
    "balrog_url": "https://aus5.mozilla.org",
    "balrog_username": "ffxbld",
    "update_channels": {
        "beta": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": False,
            "patcher_config": "moBeta-branch-patcher2.cfg",
            "update_verify_channel": "beta-localtest",
            "mar_channel_ids": [
                "firefox-mozilla-beta", "firefox-mozilla-beta",
            ],
            "channel_names": ["beta", "beta-localtest", "beta-cdntest"],
            "rules_to_update": ["firefox-beta-cdntest", "firefox-beta-localtest"],
        },
    },
}
