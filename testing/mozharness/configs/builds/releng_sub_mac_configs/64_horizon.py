import os

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'build',
        'upload-files',
        'sendchange',
        'check-test',
        'update',  # may or may not happen based on query_is_nightly()
    ],
    'stage_platform': 'mac',
    'stage_product': 'b2g',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    'enable_count_ctors': False,
    'objdir': 'obj-horizon',
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-horizon',
        'TINDERBOX_OUTPUT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/tools/buildbot/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        "SYMBOL_SERVER_HOST": "%(symbol_server_host)s",
        "SYMBOL_SERVER_SSH_KEY": "/Users/cltbld/.ssh/ffxbld_rsa",
        "SYMBOL_SERVER_USER": "ffxbld",
        "SYMBOL_SERVER_PATH": "/mnt/netapp/breakpad/symbols_ffx",
    },
    'src_mozconfig': 'b2g/graphene/config/horizon-mozconfigs/macosx64/nightly',
    'balrog_platform': 'macosx64',
    #######################
}
