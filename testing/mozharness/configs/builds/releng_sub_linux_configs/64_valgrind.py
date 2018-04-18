import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
        'valgrind-test',
        #'update',
    ],
    'stage_platform': 'linux64-valgrind',
    'enable_signing': False,
    'perfherder_extra_options': ['valgrind'],
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
    'mozconfig_variant': 'valgrind',
    #######################
    'artifact_flag_build_variant_in_try': None,
}
