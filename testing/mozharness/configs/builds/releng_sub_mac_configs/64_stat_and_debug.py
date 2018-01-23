import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'update',  # decided by query_is_nightly()
    ],
    'debug_build': True,
    'stage_platform': 'macosx64-st-an-debug',
    'build_type': 'st-an-debug',
    'tooltool_manifest_src': "browser/config/tooltool-manifests/macosx64/\
clang.manifest",
    'platform_supports_post_upload_to_latest': False,
    'enable_signing': False,
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
        # 64 bit specific
        'PATH': '/tools/python/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
    },
    'mozconfig_variant': 'debug-static-analysis',
    #######################
}
