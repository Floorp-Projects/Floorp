import os

config = {
    'vcs_share_base': os.path.join('y:', os.sep, 'hg-shared'),
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ["EN_US_BINARY_URL"],
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE', 'c:/builds/tooltool_cache'),
        'EN_US_PACKAGE_NAME': 'target.zip',
    },
    "upload_env": {
        'UPLOAD_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },

    'tooltool_manifest_src': "browser/config/tooltool-manifests/win64/releng.manifest",

    # use mozmake?
    "enable_mozmake": True,
    'exes': {},
}

