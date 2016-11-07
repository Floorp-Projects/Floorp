import os
import sys

MOZ_OBJDIR = 'obj-firefox'

config = {
    #########################################################################
    ######## WINDOWS GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_windows_32_builds.py

    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        # 'setup-mock', windows do not use mock
        'build',
        'sendchange',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'python2.7': sys.executable,
        "buildbot": [
            sys.executable,
            'c:\\mozilla-build\\buildbotve\\scripts\\buildbot'
        ],
        "make": [
            sys.executable,
            os.path.join(
                os.getcwd(), 'build', 'src', 'build', 'pymake', 'make.py'
            )
        ],
        'virtualenv': [
            sys.executable,
            'c:/mozilla-build/buildbotve/virtualenv.py'
        ],
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': False,
    'enable_ccache': False,
    'vcs_share_base': 'C:/builds/hg-shared',
    'objdir': MOZ_OBJDIR,
    'tooltool_script': [sys.executable,
                        'C:/mozilla-build/tooltool.py'],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': False,
    # debug specific
    'debug_build': True,
    'enable_talos_sendchange': False,
    # allows triggering of test jobs when --artifact try syntax is detected on buildbot
    'enable_unittest_sendchange': True,
    'max_build_output_timeout': 60 * 80,
    'perfherder_extra_options': ['artifact'],
    #########################################################################


     #########################################################################
     ###### 32 bit specific ######
    'base_name': 'WINNT_5.2_%(branch)s_Artifact_build',
    'platform': 'win32',
    'stage_platform': 'win32-debug',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': MOZ_OBJDIR,
        'PATH': 'C:/mozilla-build/nsis-3.0b1;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/c/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
        # debug-specific
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
    },
    'enable_pymake': True,
    'src_mozconfig': 'browser/config/mozconfigs/win32/debug-artifact',
    'tooltool_manifest_src': "browser/config/tooltool-manifests/win32/releng.manifest",
    #########################################################################
}
