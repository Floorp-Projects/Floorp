import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'win32-st-an-debug',
    'debug_build': True,
    'enable_signing': False,
    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/\
releng.manifest",
    'platform_supports_post_upload_to_latest': False,
    'perfherder_extra_options': ['static-analysis'],
    #### 32 bit build specific #####
    'env': {
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'PATH': 'C:/mozilla-build/nsis-3.01;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
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
