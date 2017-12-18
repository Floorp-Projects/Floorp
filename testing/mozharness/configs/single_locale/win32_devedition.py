import os
import sys

config = {
    "platform": "win32",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/win32/l10n-mozconfig-devedition",
    "bootstrap_env": {
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        "PATH": '%(abs_objdir)s\\..\\xz-5.2.3\\bin_x86-64;'
                'C:\\mozilla-build\\nsis-3.01;'
                + '%s' % (os.environ.get('path')),
        'TOOLTOOL_CACHE': 'c:/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
        'EN_US_PACKAGE_NAME': 'target.zip',
        'EN_US_PKG_INST_BASENAME': 'target.installer',
    },
    "ssh_key_dir": "~/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "c:/builds/hg-shared",

    # tooltool
    'tooltool_url': 'https://tooltool.mozilla-releng.net/',
    'tooltool_script': [sys.executable,
                        'C:/mozilla-build/tooltool.py'],
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/win32/l10n.manifest',
    # balrog credential file:
    'balrog_credentials_file': 'oauth.txt',

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
    "locales_file": "src/browser/locales/all-locales",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",

    # MAR
    "update_mar_dir": "dist\\update",  # sure?
    "application_ini": "application.ini",
    "local_mar_tool_dir": "dist\\host\\bin",
    "mar": "mar.exe",
    "mbsdiff": "mbsdiff.exe",
    "localized_mar": "firefox-%(version)s.%(locale)s.win32.complete.mar",

    # use mozmake?
    "enable_mozmake": True,
    'exes': {
        'virtualenv': [
            sys.executable,
            'c:/mozilla-build/buildbotve/virtualenv.py'
        ],
    },

    "update_channel": "aurora",
}
