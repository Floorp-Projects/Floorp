import os

config = {
    #########################################################################
    ######## MACOSX GENERIC CONFIG KEYS/VAlUES

    'default_actions': [
        'clobber',
        'build',
    ],
    "buildbot_json_path": "buildprops.json",
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'vcs_share_base': '/builds/hg-shared',
    # debug specific
    'debug_build': True,
    # allows triggering of test jobs when --artifact try syntax is detected on buildbot
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s_Artifact_build',
    'platform': 'macosx64',
    'stage_platform': 'macosx64-debug',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        # debug-specific
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'mozconfig_variant': 'debug-artifact',
    #########################################################################
}
