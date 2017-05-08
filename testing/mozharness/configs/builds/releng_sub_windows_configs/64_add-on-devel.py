import os

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        # 'setup-mock', windows do not use mock
        'build',
#        'generate-build-stats',
        'upload-files',
#        'sendchange',
        'check-test',
#        'update',
    ],
    'stage_platform': 'win64-add-on-devel',
    'build_type': 'add-on-devel',
    'enable_talos_sendchange': False,
    #### 64 bit build specific #####
    'env': {
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-firefox',
        'PATH': 'C:/mozilla-build/nsis-3.01;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TOOLTOOL_CACHE': 'c:/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
    },
    'src_mozconfig': 'browser/config/mozconfigs/win64/add-on-devel',
    #######################
}
