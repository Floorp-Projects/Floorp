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
    'stage_platform': 'win64',
    'stage_product': 'b2g',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    'enable_count_ctors': False,
    'objdir': 'obj-horizon',
    'env': {
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-horizon',
        'PATH': 'C:/mozilla-build/nsis-3.0b1;C:/mozilla-build/nsis-2.46u;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        "SYMBOL_SERVER_HOST": "%(symbol_server_host)s",
        "SYMBOL_SERVER_SSH_KEY": "/c/Users/cltbld/.ssh/ffxbld_rsa",
        "SYMBOL_SERVER_USER": "ffxbld",
        "SYMBOL_SERVER_PATH": "/mnt/netapp/breakpad/symbols_ffx",
    },
    'src_mozconfig': 'b2g/graphene/config/horizon-mozconfigs/win64/nightly',
    'balrog_platform': 'win64',
    #######################
}
