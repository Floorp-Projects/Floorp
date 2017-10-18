
config = {
    "log_name": "bump_beta_dev",
    # TODO: use real repo
    "repo": {
        "repo": "https://hg.mozilla.org/users/raliiev_mozilla.com/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "vcs_share_base": "/builds/hg-shared",
    # TODO: use real repo
    "push_dest": "ssh://hg.mozilla.org/users/raliiev_mozilla.com/tools",
    # jamun repo used for staging beta
    "shipped-locales-url": "https://hg.mozilla.org/projects/jamun/raw-file/{revision}/browser/locales/shipped-locales",
    "ignore_no_changes": True,
    "ssh_user": "ffxbld",
    "ssh_key": "~/.ssh/ffxbld_rsa",
    "archive_domain": "ftp.stage.mozaws.net",
    "archive_prefix": "https://ftp.stage.mozaws.net/pub",
    "previous_archive_prefix": "https://archive.mozilla.org/pub",
    "download_domain": "download.mozilla.org",
    "balrog_url": "http://54.90.211.22:9090",
    "balrog_username": "balrog-stage-ffxbld",
    "update_channels": {
        "beta-dev": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": True,
            # TODO - when we use a real repo, rename this file # s/MozJamun/MozBeta-dev/
            "patcher_config": "mozJamun-branch-patcher2.cfg",
            "update_verify_channel": "beta-dev-localtest",
            "mar_channel_ids": [],
            "channel_names": ["beta-dev", "beta-dev-localtest", "beta-dev-cdntest"],
            "rules_to_update": ["firefox-beta-dev-cdntest", "firefox-beta-dev-localtest"],
            "publish_rules": [32],
            "bz2_blob_suffix": "-bz2",
            "bz2_rules_to_update": ["firefox-beta-cdntest-bz2", "firefox-beta-localtest-bz2"],
            "bz2_publish_rules": [652],
            "complete_mar_filename_pattern": '%s-%s.bz2.complete.mar',
            "complete_mar_bouncer_product_pattern": '%s-%s-complete-bz2',
        }
    },
    "balrog_use_dummy_suffix": False,
}
