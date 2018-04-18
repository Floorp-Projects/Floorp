import os

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 64 bit keys/values please add them
    # below under the '64 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_linux_64_builds.py

    'default_actions': [
        'clobber',
        'build',
        'check-test',
        'update',  # decided by query_is_nightly()
    ],
    "buildbot_json_path": "buildprops.json",
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'secret_files': [
        {'filename': '/builds/gapi.data',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/gapi.data',
         'min_scm_level': 1},
        {'filename': '/builds/mozilla-desktop-geoloc-api.key',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/mozilla-desktop-geoloc-api.key',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
        {'filename': '/builds/adjust-sdk.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/adjust-sdk.token',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
        {'filename': '/builds/adjust-sdk-beta.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/adjust-sdk-beta.token',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
    ],
    'vcs_share_base': '/builds/hg-shared',
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'Linux_x86-64_%(branch)s',
    'platform': 'linux64',
    'stage_platform': 'linux64',
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
        ## 64 bit specific
        'PATH': '/usr/local/bin:/usr/lib64/ccache:/bin:'
                '/usr/bin:/usr/local/sbin:/usr/sbin:'
                '/sbin:/tools/git/bin:'
                '/home/cltbld/bin',
        ##
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s/breakpad/linux64/minidump_stackwalk',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s/minidumps',
    },
    'mozconfig_platform': 'linux64',
    'mozconfig_variant': 'nightly',
    #########################################################################
}
