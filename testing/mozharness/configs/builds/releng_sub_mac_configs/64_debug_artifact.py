import os
import sys

MOZ_OBJDIR = 'obj-firefox'

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
    'objdir': MOZ_OBJDIR,
    # debug specific
    'debug_build': True,
    'enable_talos_sendchange': False,
    # allows triggering of test jobs when --artifact try syntax is detected on buildbot
    'enable_unittest_sendchange': True,
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s_Artifact_build',
    'platform': 'macosx64',
    'stage_platform': 'macosx64-debug',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': MOZ_OBJDIR,
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        # debug-specific
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        ## 64 bit specific
        'PATH': '/tools/python/bin:/tools/buildbot/bin:/opt/local/bin:/usr/bin:'
                '/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin',
        ##
    },
    'src_mozconfig': 'browser/config/mozconfigs/macosx64/debug-artifact',
    #########################################################################
}
