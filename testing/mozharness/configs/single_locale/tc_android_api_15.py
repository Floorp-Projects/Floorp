import os

config = {
    "locales_file": "src/mobile/android/locales/all-locales",
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "mozconfig": "src/mobile/android/config/mozconfigs/android-api-15/l10n-nightly",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/src",
    },
    #"tooltool_servers": ['http://relengapi/tooltool/'],

    #"bootstrap_env": {
    #    "NO_MERCURIAL_SETUP_CHECK": "1",
    #    "MOZ_OBJDIR": "obj-l10n",
    #    "EN_US_BINARY_URL": "%(en_us_binary_url)s",
    #    "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
    #    "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
    #    "DIST": "%(abs_objdir)s",
    #    "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
    #    "L10NBASEDIR": "../../l10n",
    #    "MOZ_MAKE_COMPLETE_MAR": "1",
    #    'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE'),
    #},
    "upload_env": {
        'UPLOAD_HOST': 'localhost',
        'UPLOAD_PATH': '/home/worker/artifacts/',
    },
    "mozilla_dir": "src/",
}
