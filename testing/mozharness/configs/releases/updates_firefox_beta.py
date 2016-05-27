
config = {
    "log_name": "updates_beta",
    "repo": {
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/build/tools",
    "shipped-locales-url": "https://hg.mozilla.org/releases/mozilla-beta/raw-file/{revision}/browser/locales/shipped-locales",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "archive_domain": "archive.mozilla.org",
    "archive_prefix": "https://archive.mozilla.org/pub",
    "previous_archive_prefix": "https://archive.mozilla.org/pub",
    "download_domain": "download.mozilla.org",
    "balrog_url": "https://aus5.mozilla.org",
    "balrog_username": "ffxbld",
    "update_channels": {
        "beta": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": True,
            "patcher_config": "mozBeta-branch-patcher2.cfg",
            "update_verify_channel": "beta-localtest",
            "mar_channel_ids": [],
            "channel_names": ["beta", "beta-localtest", "beta-cdntest"],
            "rules_to_update": ["firefox-beta-cdntest", "firefox-beta-localtest"],
            "publish_rules": ["firefox-beta"],
        },
    },
    "balrog_use_dummy_suffix": False,
}
