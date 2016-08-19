config = {
    "nightly_build": False,
    "branch": "try",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly",
    "latest_mar_dir": '/pub/firefox/nightly/latest-mozilla-central-l10n',
    "update_gecko_source_to_enUS": False,

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s",

    # repositories
    "mozilla_dir": "try",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/try",
        "revision": "%(revision)s",
        "dest": "try",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-central",
        "clone_by_revision": True,
        "clone_with_purge": True,
    }],
    # purge options
    'is_automation': True,
    "upload_env": {
        "UPLOAD_USER": "trybld",
        # ssh_key_dir is defined per platform: it is "~/.ssh" for every platform
        # except when mock is in use, in this case, ssh_key_dir is
        # /home/mock_mozilla/.ssh
        "UPLOAD_SSH_KEY": "%(ssh_key_dir)s/trybld_dsa",
        "UPLOAD_HOST": "upload.trybld.productdelivery.%(upload_environment)s.mozaws.net",
        "POST_UPLOAD_CMD": "post_upload.py --who %(who)s --builddir %(branch)s-%(platform)s --tinderbox-builds-dir %(who)s-%(revision)s -p %(stage_product)s -i %(buildid)s --revision %(revision)s --release-to-try-builds %(post_upload_extra)s",
        "UPLOAD_TO_TEMP": "1"
    },
}
