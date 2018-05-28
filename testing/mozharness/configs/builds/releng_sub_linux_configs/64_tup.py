import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'linux64-tup-opt',
    'enable_talos_sendchange': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 64 bit specific
        'PATH': '/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
        'LD_LIBRARY_PATH': '%(abs_obj_dir)s/dist/bin',
        'TINDERBOX_OUTPUT': '1',

        # sccache doesn't work yet with tup due to the way the server is
        # spawned, and because the server does the file I/O
        'SCCACHE_DISABLE': '1',
    },
    'mozconfig_variant': 'tup',
    'disable_package_metrics': True, # TODO: the package needs to be created for this to work
    'artifact_flag_build_variant_in_try': None,
}
