import os

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_linux_64_builds.py

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
    # allows triggering of dependent jobs when --artifact try syntax is detected on buildbot
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 32 bit specific ######
    'base_name': 'Linux_%(branch)s_Artifact_build',
    'platform': 'linux',
    'stage_platform': 'linux',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        # 32 bit specific
        'PATH': '/usr/local/bin:/usr/lib/ccache:\
/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:\
/tools/python27/bin:/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib",
    },
    'mozconfig_variant': 'artifact',
    #########################################################################
}
