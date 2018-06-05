# This config references releases/l10n/mozilla-release instead of l10n-central
# because 52 predates cross-channel localization, which rides the train
# with 57.
# If you copy this config for the ESR following 57, change hg_l10n_base
# to l10n-central.
config = {
    "nightly_build": True,
    "branch": "mozilla-esr52",
    "en_us_binary_url": "https://archive.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-esr52/",
    "update_channel": "esr",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-release",

    # repositories
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/releases/mozilla-esr52",
        "revision": "%(revision)s",
        "dest": "mozilla-esr52",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
    }],
    # purge options
    'purge_minsize': 12,
    'is_automation': True,
}
