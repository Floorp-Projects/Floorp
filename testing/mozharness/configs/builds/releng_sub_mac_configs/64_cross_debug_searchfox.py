import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'update',  # decided by query_is_nightly()
    ],
    'stage_platform': 'macosx64-searchfox-debug',
    'debug_build': True,
    'perfherder_extra_options': ['static-analysis'],
    #### 64 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # Disable sccache because otherwise we won't index the files that
        # sccache optimizes away compilation for
        'SCCACHE_DISABLE': '1',
        # 64 bit specific
        'PATH': '/tools/python/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
    },
    'mozconfig_variant': 'debug-searchfox',
    'artifact_flag_build_variant_in_try': None,
    #######################
}
