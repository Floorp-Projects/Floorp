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
    ],
    'stage_platform': 'linux-rusttests',
    'enable_signing': False,
    'enable_talos_sendchange': False,
    #### 32 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': MOZ_OBJDIR,
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        # 32 bit specific
        'PATH': '/usr/local/bin:/usr/lib/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': '/tools/gcc-4.3.3/installed/lib:\
%s/dist/bin' % (MOZ_OBJDIR,),
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TINDERBOX_OUTPUT': '1',
    },
    'src_mozconfig': 'browser/config/mozconfigs/linux32/rusttests',
    'artifact_flag_build_variant_in_try': None,
    #######################
}
