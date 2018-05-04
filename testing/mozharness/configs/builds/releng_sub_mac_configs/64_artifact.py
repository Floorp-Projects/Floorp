import os

config = {
    #########################################################################
    ######## MACOSX GENERIC CONFIG KEYS/VAlUES

    'default_actions': [
        'clobber',
        'build',
    ],
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'vcs_share_base': '/builds/hg-shared',
    # allows triggering of dependent jobs when --artifact try syntax is detected
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s_Artifact_build',
    'platform': 'macosx64',
    'stage_platform': 'macosx64',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'CHOWN_ROOT': '~/bin/chown_root',
        'CHOWN_REVERT': '~/bin/chown_revert',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'mozconfig_variant': 'artifact',
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/macosx64/releng.manifest',
    #########################################################################
}
