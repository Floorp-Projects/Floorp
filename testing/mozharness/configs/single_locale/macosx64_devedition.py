import os

config = {
    # mozconfig file to use, it depends on branch and platform names
    "platform": "macosx64",
    "stage_product": "firefox",
    "update_platform": "Darwin_x86_64-gcc3",
    "mozconfig": "%(branch)s/browser/config/mozconfigs/macosx64/l10n-mozconfig-devedition",
    "bootstrap_env": {
        "SHELL": '/bin/bash',
        "MOZ_OBJDIR": "obj-l10n",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "MOZ_PKG_PLATFORM": "mac",
        # "IS_NIGHTLY": "yes",
        "DIST": "%(abs_objdir)s",
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
    },
    "ssh_key_dir": "~/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-l10n",
    "js_src_dir": "js/src",
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
    "merge_locales": True,

    # MAR
    "previous_mar_dir": "dist/previous",
    "current_mar_dir": "dist/current",
    "update_mar_dir": "dist/update",  # sure?
    "previous_mar_filename": "previous.mar",
    "current_work_mar_dir": "current.work",
    "package_base_dir": "dist/l10n-stage",
    "application_ini": "Contents/Resources/application.ini",
    "buildid_section": 'App',
    "buildid_option": "BuildID",
    "unpack_script": "tools/update-packaging/unwrap_full_update.pl",
    "incremental_update_script": "tools/update-packaging/make_incremental_update.sh",
    "balrog_release_pusher_script": "scripts/updates/balrog-release-pusher.py",
    "update_packaging_dir": "tools/update-packaging",
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",
    "current_mar_filename": "firefox-%(version)s.%(locale)s.mac.complete.mar",
    "complete_mar": "firefox-%(version)s.en-US.mac.complete.mar",
    "localized_mar": "firefox-%(version)s.%(locale)s.mac.complete.mar",
    "partial_mar": "firefox-%(version)s.%(locale)s.mac.partial.%(from_buildid)s-%(to_buildid)s.mar",
    'installer_file': "firefox-%(version)s.en-US.mac.dmg",
}
