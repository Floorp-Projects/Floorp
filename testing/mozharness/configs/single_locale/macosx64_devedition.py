import os

config = {
    # mozconfig file to use, it depends on branch and platform names
    "platform": "macosx64",
    "stage_product": "firefox",
    "mozconfig": "%(branch)s/browser/config/mozconfigs/macosx64/l10n-mozconfig-devedition",
    "bootstrap_env": {
        "SHELL": '/bin/bash',
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "MOZ_PKG_PLATFORM": "mac",
        # "IS_NIGHTLY": "yes",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        'EN_US_PACKAGE_NAME': 'target.dmg',
    },
    "ssh_key_dir": "~/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

    "upload_env_extra": {
        "MOZ_PKG_PLATFORM": "mac",
    },

    # balrog credential file:
    'balrog_credentials_file': 'oauth.txt',

    # l10n
    "ignore_locales": ["en-US", "ja"],
    "l10n_dir": "l10n",
    "locales_file": "%(branch)s/browser/locales/all-locales",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",

    # MAR
    "update_mar_dir": "dist/update",  # sure?
    "application_ini": "Contents/Resources/application.ini",
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",
    "localized_mar": "firefox-%(version)s.%(locale)s.mac.complete.mar",
}
