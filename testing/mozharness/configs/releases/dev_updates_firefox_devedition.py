
config = {
    "log_name": "updates_devedition",
    # TODO: use real repo
    "repo": {
        "repo": "https://hg.mozilla.org/users/asasaki_mozilla.com/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "vcs_share_base": "/builds/hg-shared",
    # TODO: use real repo
    "push_dest": "ssh://hg.mozilla.org/users/asasaki_mozilla.com/tools",
    # maple repo used for staging beta
    "shipped-locales-url": "https://hg.mozilla.org/projects/maple/raw-file/{revision}/browser/locales/shipped-locales",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "archive_domain": "ftp.stage.mozaws.net",
    "archive_prefix": "https://ftp.stage.mozaws.net/pub",
    "previous_archive_prefix": "https://archive.mozilla.org/pub",
    "download_domain": "download.mozilla.org",
    "balrog_url": "https://aus4.stage.mozaws.net/",
    "balrog_username": "balrog-stage-ffxbld",
    "update_channels": {
        "aurora": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": True,
            # TODO - when we use a real repo, rename this file # s/MozJamun/Mozbeta/
            "patcher_config": "mozDevedition-branch-patcher2.cfg",
            "patcher_config_product_override": "firefox",
            "update_verify_channel": "aurora-localtest",
            "mar_channel_ids": [],
            "channel_names": ["aurora", "aurora-localtest", "aurora-cdntest"],
            "rules_to_update": ["devedition-cdntest", "devedition-localtest"],
            "publish_rules": [10],
        }
    },
    "balrog_use_dummy_suffix": False,
    "stage_product": "devedition",
    "bouncer_product": "devedition",
}
