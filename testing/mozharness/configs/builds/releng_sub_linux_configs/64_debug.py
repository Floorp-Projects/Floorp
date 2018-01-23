import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'linux64-debug',
    'debug_build': True,
    'enable_signing': False,
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 64 bit specific
        'PATH': '/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': '/tools/gcc-4.3.3/installed/lib64:%(abs_obj_dir)s/dist/bin',
        'TINDERBOX_OUTPUT': '1',
    },
    'mozconfig_variant': 'debug',
    #######################
}
