config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'package-source',
    ],
    'stage_platform': 'source',  # Not used, but required by the script
    'purge_minsize': 3,
    'buildbot_json_path': 'buildprops.json',
    'app_ini_path': 'FAKE',  # Not used, but required by the script
    'objdir': 'obj-firefox',
    'env': {
        'MOZ_OBJDIR': 'obj-firefox',
        'TINDERBOX_OUTPUT': '1',
        'LC_ALL': 'C',
    },
}
