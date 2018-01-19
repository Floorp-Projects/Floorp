config = {
    "platform": "linux",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/linux32/l10n-mozconfig",
    "bootstrap_env": {
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        'EN_US_PACKAGE_NAME': 'target.tar.bz2',
    },
    "ssh_key_dir": "/home/mock_mozilla/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

    # balrog credential file:
    'balrog_credentials_file': 'oauth.txt',

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
    "locales_file": "src/browser/locales/all-locales",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",

    # MAR
    "update_mar_dir": "dist/update",  # sure?
    "application_ini": "application.ini",
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",
    "localized_mar": "firefox-%(version)s.%(locale)s.linux-i686.complete.mar",
}
