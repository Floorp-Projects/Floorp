import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
    ],
    'stage_platform': 'linux64-rusttests-debug',
    'debug_build': True,
    'enable_signing': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 64 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/home/cltbld/bin',
        'LD_LIBRARY_PATH': '%(abs_obj_dir)s/dist/bin',
        'TINDERBOX_OUTPUT': '1',
    },
    'mozconfig_variant': 'rusttests-debug',
    'artifact_flag_build_variant_in_try': None,
}
