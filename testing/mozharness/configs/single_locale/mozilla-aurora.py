import os

config = {
    "nightly_build": True,
    "branch": "mozilla-aurora",
    "update_channel": "aurora",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-aurora",

    # mar - passed in environ from taskcluster
    "mar_tools_url": os.environ["MAR_TOOLS_URL"],

    # purge options
    'is_automation': True,
}
