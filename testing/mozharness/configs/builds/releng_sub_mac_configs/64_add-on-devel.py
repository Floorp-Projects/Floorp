import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
#        'update',
    ],
    'stage_platform': 'macosx64-add-on-devel',
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'mozconfig_variant': 'add-on-devel',
    #######################
}
