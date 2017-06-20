
config = {
    "log_name": "updates_devedition",
    "repo": {
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "vcs_share_base": "/builds/hg-shared",
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
    "balrog_username": "balrog-ffxbld",
    "update_channels": {
        "aurora": {
            "version_regex": r"^.*$",
            "requires_mirrors": True,
            "patcher_config": "mozDevedition-branch-patcher2.cfg",
            # Allow to override the patcher config product name, regardless
            # the value set by buildbot properties
            "patcher_config_product_override": "firefox",
            "update_verify_channel": "aurora-localtest",
            "mar_channel_ids": [],
            "channel_names": ["aurora", "aurora-localtest", "aurora-cdntest"],
            "rules_to_update": ["devedition-cdntest", "devedition-localtest"],
            "publish_rules": [10],
        },
    },
    "balrog_use_dummy_suffix": False,
    "stage_product": "devedition",
    "bouncer_product": "devedition",
}
