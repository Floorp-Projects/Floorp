import os

EN_US_BINARY_URL = "%(en_us_binary_url)s"

config = {
    "locales_file": "src/browser/locales/all-locales",
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE'),
    },
    "upload_env": {
        'UPLOAD_HOST': 'localhost',
        'UPLOAD_PATH': '/builds/worker/artifacts/',
    },

    "tooltool_url": 'http://relengapi/tooltool/',
}

