# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "log_name": "updates_release_dev",
    # TODO: use real repo
    "repo": {
        "repo": "https://hg.mozilla.org/users/stage-ffxbld/tools",
        "branch": "default",
        "dest": "tools",
        "vcs": "hg",
    },
    "vcs_share_base": "/builds/hg-shared",
    # TODO: use real repo
    "push_dest": "ssh://hg.mozilla.org/users/stage-ffxbld/tools",
    # jamun  repo used for staging release
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
        "beta": {
            "version_regex": r"^(\d+\.\d+(b\d+)?)$",
            "requires_mirrors": False,
            "patcher_config": "mozDate-branch-patcher2.cfg",
            "update_verify_channel": "beta-localtest",
            "mar_channel_ids": [
                "firefox-mozilla-beta",
                "firefox-mozilla-release",
            ],
            "channel_names": ["beta", "beta-localtest", "beta-cdntest"],
            "rules_to_update": ["firefox-beta-cdntest", "firefox-beta-localtest"],
            "publish_rules": [32],
            "schedule_asap": True,
        },
        "release": {
            "version_regex": r"^\d+\.\d+(\.\d+)?$",
            "requires_mirrors": True,
            "patcher_config": "mozJamun-branch-patcher2.cfg",
            "update_verify_channel": "release-localtest",
            "mar_channel_ids": [],
            "channel_names": ["release", "release-localtest", "release-cdntest"],
            "rules_to_update": ["firefox-release-cdntest", "firefox-release-localtest"],
            "publish_rules": [145],
        },
    },
    "balrog_use_dummy_suffix": False,
}
