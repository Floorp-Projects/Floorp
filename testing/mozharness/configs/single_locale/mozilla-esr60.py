import os

config = {
    "nightly_build": True,
    "branch": "mozilla-esr60",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-esr60/",
    "update_channel": "esr",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": os.environ["MAR_TOOLS_URL"],

    # repositories
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/releases/mozilla-esr60",
        "revision": "%(revision)s",
        "dest": "mozilla-esr60",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    }],
    # purge options
    'purge_minsize': 12,
    'is_automation': True,
}
