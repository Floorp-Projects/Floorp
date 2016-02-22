config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'package-source',
        'generate-source-signing-manifest',
    ],
    'stage_platform': 'source',  # Not used, but required by the script
    'buildbot_json_path': 'buildprops.json',
    'app_ini_path': 'FAKE',  # Not used, but required by the script
    'objdir': 'obj-firefox',
    'env': {
        'MOZ_OBJDIR': 'obj-firefox',
        'TINDERBOX_OUTPUT': '1',
        'LC_ALL': 'C',
    },
    'src_mozconfig': 'browser/config/mozconfigs/linux64/source',
}
