
config = {
    "log_name": "bump_beta_dev",
    # TODO: use real repo
    "repo": {
        "repo": "https://hg.mozilla.org/users/raliiev_mozilla.com/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    # TODO: use real repo
    "push_dest": "ssh://hg.mozilla.org/users/raliiev_mozilla.com/tools",
    # date repo used for staging beta
    "shipped-locales-url": "https://hg.mozilla.org/projects/date/raw-file/{revision}/browser/locales/shipped-locales",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "archive_domain": "ftp.stage.mozaws.net",
    "archive_prefix": "https://ftp.stage.mozaws.net/pub",
    "previous_archive_prefix": "https://archive.mozilla.org/pub",
    "download_domain": "download.mozilla.org",
    "balrog_url": "http://ec2-54-241-39-23.us-west-1.compute.amazonaws.com",
    "balrog_username": "stage-ffxbld",
    "update_channels": {
        "beta-dev": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": True,
            # TODO - when we use a real repo, rename this file # s/MozDate/MozBeta-dev/
            "patcher_config": "mozDate-branch-patcher2.cfg",
            "update_verify_channel": "beta-dev-localtest",
            "mar_channel_ids": [],
            "channel_names": ["beta-dev", "beta-dev-localtest", "beta-dev-cdntest"],
            "rules_to_update": ["firefox-beta-dev-cdntest", "firefox-beta-dev-localtest"],
            "publish_rules": ["firefox-beta"],
        }
    },
    "balrog_use_dummy_suffix": False,
}
