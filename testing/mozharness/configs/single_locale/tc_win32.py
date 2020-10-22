import os

config = {
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "%(abs_obj_dir)s",
        "EN_US_BINARY_URL": os.environ["EN_US_BINARY_URL"],
        "DIST": "%(abs_obj_dir)s",
        "L10NBASEDIR": "../../l10n",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE'),
        'EN_US_PACKAGE_NAME': 'target.zip',
    },

    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/releng.manifest",
}
