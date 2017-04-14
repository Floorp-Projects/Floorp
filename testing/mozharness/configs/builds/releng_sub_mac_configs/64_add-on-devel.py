import os

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
#        'setup-mock',
        'build',
#        'generate-build-stats',
        'upload-files',
#        'sendchange',
        'check-test',
#        'update',
    ],
    'stage_platform': 'macosx64-add-on-devel',
    'publish_nightly_en_US_routes': False,
    'build_type': 'add-on-devel',
    'platform_supports_post_upload_to_latest': False,
    'objdir': 'obj-firefox',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
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
        ## 64 bit specific
        'PATH': '/tools/python/bin:/tools/buildbot/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'src_mozconfig': 'browser/config/mozconfigs/macosx64/add-on-devel',
    #######################
}
