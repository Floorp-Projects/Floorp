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
    'env': {
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'LC_ALL': 'C',
    },
    'mozconfig_variant': 'source',
}
