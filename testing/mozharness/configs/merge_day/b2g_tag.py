LIVE_B2G_BRANCHES = {
    "mozilla-b2g44_v2_5": {
        "gaia_branch": "v2.5",
        "tag_name": "B2G_2_5_%(DATE)s_MERGEDAY",
    },
}

config = {
    "log_name": "b2g_tag",

    "gaia_mapper_base_url": "http://cruncher/mapper/gaia/git",
    "gaia_url": "git@github.com:mozilla-b2g/gaia.git",
    "hg_base_pull_url": "https://hg.mozilla.org/releases",
    "hg_base_push_url": "ssh://hg.mozilla.org/releases",
    "b2g_branches": LIVE_B2G_BRANCHES,

    # Disallow sharing, since we want pristine .hg directories.
    "vcs_share_base": None,
    "hg_share_base": None,

    # any hg command line options
    "exes": {
        "hg": [
            "hg", "--config",
            "hostfingerprints.hg.mozilla.org=73:7F:EF:AB:68:0F:49:3F:88:91:F0:B7:06:69:FD:8F:F2:55:C9:56",
        ],
    }
}
