import os

config = {
    'default_actions': [
        'clobber',
        'build',
    ],
    'stage_platform': 'linux64-asan-debug',
    'debug_build': True,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
    },
    'mozconfig_variant': 'debug-asan',
    #######################
}
