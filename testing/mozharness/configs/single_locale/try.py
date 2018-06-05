import os

config = {
    "nightly_build": False,
    "branch": "try",
    "en_us_binary_url": "http://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central",
    "update_channel": "nightly-try",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar - passed in environ from taskcluster
    "mar_tools_url": os.environ["MAR_TOOLS_URL"],

    # purge options
    'is_automation': True,
}
