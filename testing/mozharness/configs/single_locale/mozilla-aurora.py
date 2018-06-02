import os

config = {
    "nightly_build": True,
    "branch": "mozilla-aurora",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora/",
    "update_channel": "aurora",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/mozilla-aurora",

    # mar
    "mar_tools_url": os.environ.get(
        "MAR_TOOLS_URL",
        # Default to fetching from ftp rather than setting an environ var
        "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora/mar-tools/%(platform)s"
    ),

    # purge options
    'is_automation': True,
}
