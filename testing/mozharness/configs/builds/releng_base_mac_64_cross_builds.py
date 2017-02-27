import os
import sys

config = {
    #########################################################################
    ######## MACOSX CROSS GENERIC CONFIG KEYS/VAlUES

    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'build',
        'generate-build-stats',
        'update',  # decided by query_is_nightly()
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'python2.7': sys.executable,
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'secret_files': [
        {'filename': '/builds/gapi.data',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/gapi.data',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
        {'filename': '/builds/mozilla-desktop-geoloc-api.key',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/mozilla-desktop-geoloc-api.key',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
    ],
    'enable_ccache': True,
    'enable_check_test': False,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': 'obj-firefox/',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': False,
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s',
    'platform': 'macosx64',
    'stage_platform': 'macosx64',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-firefox',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        ##
    },
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/Users/cltbld/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s/breakpad/linux64/minidump_stackwalk',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s/minidumps',
    },
    'src_mozconfig': 'browser/config/mozconfigs/macosx64/nightly',
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/macosx64/cross-releng.manifest',
    #########################################################################
}
