config = {
    "app_name": "browser",
    "mozconfig_platform": "linux64",
    "mozconfig_variant": "l10n-mozconfig-devedition",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",

    # MAR
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",

    "update_channel": "aurora",
}
