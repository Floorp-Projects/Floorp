import os

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'build',
        'upload-files',
        'sendchange',
        'check-test',
        'update',  # may or may not happen based on query_is_nightly()
    ],
    'stage_platform': 'linux64',
    'stage_product': 'b2g',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    'enable_count_ctors': False,
    'objdir': 'obj-graphene',
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-graphene',
        'TINDERBOX_OUTPUT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
    },
    'src_mozconfig': 'b2g/graphene/config/mozconfigs/linux64/nightly',
    'balrog_platform': 'linux64',
    #######################
}
