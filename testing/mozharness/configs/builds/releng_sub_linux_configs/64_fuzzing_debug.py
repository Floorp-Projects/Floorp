import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
    ],
    'stage_platform': 'linux64-fuzzing-debug',
    'debug_build': True,
    'enable_signing': False,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 64 bit specific
        'PATH': '/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
        'LD_LIBRARY_PATH': '%(abs_obj_dir)s/dist/bin',
        'TINDERBOX_OUTPUT': '1',
    },
    'mozconfig_variant': 'debug-fuzzing',
    #######################
}
