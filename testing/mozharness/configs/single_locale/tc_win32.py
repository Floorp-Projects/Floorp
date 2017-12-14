import os

EN_US_BINARY_URL = "%(en_us_binary_url)s"

config = {
    "locales_file": "src/browser/locales/all-locales",
    "tools_repo": "https://hg.mozilla.org/build/tools",
    'vcs_share_base': os.path.join('y:', os.sep, 'hg-shared'),
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        # EN_US_INSTALLER_BINARY_URL falls back on EN_US_BINARY_URL
        "EN_US_INSTALLER_BINARY_URL": os.environ.get(
            "EN_US_INSTALLER_BINARY_URL", os.environ.get(
                "EN_US_BINARY_URL", EN_US_BINARY_URL)),
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE', 'c:/builds/tooltool_cache'),
        'EN_US_PACKAGE_NAME': 'target.zip',
        'EN_US_PKG_INST_BASENAME': 'target.installer',
    },
    "upload_env": {
        'UPLOAD_HOST': 'localhost',
        'UPLOAD_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },

    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/releng.manifest",
    "mozilla_dir": "src/",
    "simple_name_move": True,
    "update_gecko_source_to_enUS": False,

    # Override the buildbot path to not read it
    "buildbot_json_path": None,
}
