import os

config = {
    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'clobber',
        'build',
    ],
    "buildbot_json_path": "buildprops.json",
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': False,
    'vcs_share_base': '/builds/hg-shared',
    'enable_count_ctors': True,
    'perfherder_extra_options': ['static-analysis'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'Linux_x86-64_%(branch)s_Searchfox',
    'platform': 'linux64',
    'stage_platform': 'linux64-searchfox-opt',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/worker/tooltool-cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        # Disable sccache because otherwise we won't index the files that
        # sccache optimizes away compilation for
        'SCCACHE_DISABLE': '1',
        ## 64 bit specific
        'PATH': '/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib64",
        ##
    },
    # This doesn't actually inherit from anything.
    'mozconfig_platform': 'linux64',
    'mozconfig_variant': 'debug-searchfox-clang',
    #######################
    'artifact_flag_build_variant_in_try': None,
}
