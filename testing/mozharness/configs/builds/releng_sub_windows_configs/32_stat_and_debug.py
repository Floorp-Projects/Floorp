import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'win32-st-an-debug',
    'debug_build': True,
    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/\
releng.manifest",
    #### 32 bit build specific #####
    'env': {
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'PATH': 'C:/mozilla-build/nsis-3.01;C:/mozilla-build/python27;'
                '%s' % (os.environ.get('path')),
        'TINDERBOX_OUTPUT': '1',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TOOLTOOL_CACHE': 'c:/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
    },
    'mozconfig_variant': 'debug-static-analysis',
    'purge_minsize': 9,
    'artifact_flag_build_variant_in_try': None,
    #######################
}
