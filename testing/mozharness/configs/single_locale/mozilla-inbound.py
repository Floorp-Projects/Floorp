import os

config = {
    "nightly_build": True,
    "branch": "mozilla-inbound",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": os.environ.get(
        "MAR_TOOLS_URL",
        # Default to fetching from ftp rather than setting an environ var
        "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s"
    ),

    # repositories
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/integration/mozilla-inbound",
        "revision": "%(revision)s",
        "dest": "mozilla-inbound",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    }],
    # purge options
    'is_automation': True,
}
