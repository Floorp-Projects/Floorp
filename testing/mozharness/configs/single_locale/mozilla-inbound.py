import os

config = {
    "nightly_build": True,
    "branch": "mozilla-inbound",
    "update_channel": "nightly",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar - passed in environ from taskcluster
    "mar_tools_url": os.environ["MAR_TOOLS_URL"],

    # purge options
    'is_automation': True,
}
