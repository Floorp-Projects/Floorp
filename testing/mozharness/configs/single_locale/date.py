import os

config = {
    "nightly_build": True,
    "branch": "date",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly-date",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "mar_tools_url": os.environ.get(
        "MAR_TOOLS_URL",
        # Buildbot l10n fetches from ftp rather than setting an environ var
        "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s"
    ),

    # repositories
    "mozilla_dir": "date",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/projects/date",
        "revision": "%(revision)s",
        "dest": "date",
    }],
    # purge options
    'is_automation': True,
}
