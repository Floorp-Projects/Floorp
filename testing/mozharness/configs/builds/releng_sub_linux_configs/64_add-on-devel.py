import os

config = {
    'default_actions': [
        'clobber',
        'build',
        'check-test',
        # 'update',
    ],
    'stage_platform': 'linux64-add-on-devel',
    'publish_nightly_en_US_routes': False,
    'build_type': 'add-on-devel',
    'platform_supports_post_upload_to_latest': False,
    'enable_signing': False,
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
        ## 64 bit specific
        'PATH': '/builds/worker/workspace/build/src/gcc/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
    },
    'mozconfig_variant': 'add-on-devel',
    #######################
}
