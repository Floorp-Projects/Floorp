import os

MOZ_OBJDIR = 'obj-firefox'

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
        # 'generate-build-stats',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'linux64-stylo',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': MOZ_OBJDIR,
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 64 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': '/tools/gcc-4.3.3/installed/lib64:\
%s/dist/bin' % (MOZ_OBJDIR,),
        'TINDERBOX_OUTPUT': '1',
    },
    'src_mozconfig': 'browser/config/mozconfigs/linux64/stylo',
}
