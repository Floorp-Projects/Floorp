config = {
    "platform": "linux64",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/linux64/l10n-mozconfig-devedition",
    "ssh_key_dir": "/home/mock_mozilla/.ssh",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

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
    "localized_mar": "firefox-%(version)s.%(locale)s.linux-x86_64.complete.mar",

    "update_channel": "aurora",
}
