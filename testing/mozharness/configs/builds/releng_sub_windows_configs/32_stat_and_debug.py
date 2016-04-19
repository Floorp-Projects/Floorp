import os

MOZ_OBJDIR = 'obj-firefox'

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        # 'setup-mock', windows do not use mock
        'build',
        # 'generate-build-stats',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'win32-st-an-debug',
    'debug_build': True,
    'enable_signing': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/\
clang.manifest",
    'platform_supports_post_upload_to_latest': False,
    'objdir': MOZ_OBJDIR,
    #### 32 bit build specific #####
    'env': {
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-firefox',
        'PATH': 'C:/mozilla-build/nsis-3.0a2;C:/mozilla-build/nsis-2.46u;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TOOLTOOL_CACHE': '/c/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
    },
    'src_mozconfig': 'browser/config/mozconfigs/win32/debug-static-analysis',
    'purge_minsize': 9,
    #######################
}
