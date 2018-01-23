config = {
    "platform": "win32",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/win32/l10n-mozconfig",
    "ssh_key_dir": "~/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "c:/builds/hg-shared",

    # tooltool
    'tooltool_url': 'https://tooltool.mozilla-releng.net/',
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/win32/l10n.manifest',

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
    'exes': {}
}
