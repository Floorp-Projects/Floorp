import os
import sys

config = {
    #########################################################################
    ######## MACOSX GENERIC CONFIG KEYS/VAlUES

    'default_actions': [
        'clobber',
        'clone-tools',
        # 'setup-mock',
        'checkout-sources',
        'build',
        'sendchange',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'python2.7': sys.executable,
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': False,
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': 'obj-firefox',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': False,
    # allows triggering of dependent jobs when --artifact try syntax is detected on buildbot
    'enable_unittest_sendchange': True,
    'enable_talos_sendchange': True,
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s_Artifact_build',
    'platform': 'macosx64',
    'stage_platform': 'macosx64',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-firefox',
        'CHOWN_ROOT': '~/bin/chown_root',
        'CHOWN_REVERT': '~/bin/chown_revert',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/tools/buildbot/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'src_mozconfig': 'browser/config/mozconfigs/macosx64/artifact',
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/macosx64/releng.manifest',
    #########################################################################
}
