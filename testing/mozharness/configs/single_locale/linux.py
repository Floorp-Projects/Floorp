config = {
    "platform": "linux",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/linux32/l10n-mozconfig",
    "ssh_key_dir": "/home/mock_mozilla/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
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
