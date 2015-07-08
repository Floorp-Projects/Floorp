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
    'stage_platform': 'macosx64-mulet',
    'stage_product': 'b2g',
    'tooltool_manifest_src': "b2g/dev/config/tooltool-manifests/macosx64/releng.manifest",
    'objdir': 'obj-firefox',
    'enable_signing': False,
    'enable_unittest_sendchange': False,
    'enable_talos_sendchange': False,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-firefox',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/tools/buildbot/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'src_mozconfig': 'b2g/dev/config/mozconfigs/macosx64/mulet',
    'purge_minsize': 10,
    #######################
}
