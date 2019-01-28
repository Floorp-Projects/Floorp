import os

config = {
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ["EN_US_BINARY_URL"],
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE'),
    },
    "upload_env": {
        'UPLOAD_PATH': '/builds/worker/artifacts/',
    },

    "vcs_share_base": "/builds/hg-shared",
}
