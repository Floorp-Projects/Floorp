
config = {
    "log_name": "updates_esr45",
    "repo": {
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "push_dest": "ssh://hg.mozilla.org/build/tools",
    "shipped-locales-url": "https://hg.mozilla.org/releases/mozilla-esr45/raw-file/{revision}/browser/locales/shipped-locales",
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
        "esr": {
            "version_regex": r".*",
            "requires_mirrors": True,
            "patcher_config": "mozEsr45-branch-patcher2.cfg",
            "update_verify_channel": "esr-localtest",
            "mar_channel_ids": [],
            "channel_names": ["esr", "esr-localtest", "esr-cdntest"],
            "rules_to_update": ["esr45-cdntest", "esr45-localtest"],
            "publish_rules": ["esr"],
        },
    },
    "balrog_use_dummy_suffix": False,
}
