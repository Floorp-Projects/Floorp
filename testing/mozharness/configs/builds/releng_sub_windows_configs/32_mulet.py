import os

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'build',
        'upload-files',
        'sendchange',
    ],
    'stage_platform': 'win32-mulet',
    'publish_nightly_en_US_routes': False,
    'build_type': 'mulet-opt',
    'stage_product': 'b2g',
    'tooltool_manifest_src': "b2g/dev/config/tooltool-manifests/win32/releng.manifest",
    'enable_signing': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    #### 32 bit build specific #####
    'env': {
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-firefox',
        'PATH': 'C:/mozilla-build/nsis-3.0b1;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TOOLTOOL_CACHE': '/c/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
    },
    'src_mozconfig': 'b2g/dev/config/mozconfigs/win32/mulet',
    #######################
}
