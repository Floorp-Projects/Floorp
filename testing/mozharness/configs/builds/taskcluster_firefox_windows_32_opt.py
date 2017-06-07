import os
import sys

config = {
    #########################################################################
    ######## WINDOWS GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced between:
    # - taskcluster_firefox_win32_debug
    # - taskcluster_firefox_win32_opt
    # - taskcluster_firefox_win64_debug
    # - taskcluster_firefox_win64_opt
    # - taskcluster_firefox_win32_clang
    # - taskcluster_firefox_win32_clang_debug
    # - taskcluster_firefox_win64_clang
    # - taskcluster_firefox_win64_clang_debug

    'default_actions': [
        'clone-tools',
        'build',
        'check-test',
    ],
    'exes': {
        'virtualenv': [
            sys.executable,
            os.path.join(
                os.getcwd(), 'build', 'src', 'third_party', 'python', 'virtualenv', 'virtualenv.py'
            )
        ],
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'vcs_share_base': os.path.join('y:', os.sep, 'hg-shared'),
    'objdir': 'obj-firefox',
    'tooltool_script': [
      sys.executable,
      os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')
    ],
    'tooltool_bootstrap': 'setup.sh',
    'enable_count_ctors': False,
    'max_build_output_timeout': 60 * 80,
    #########################################################################


     #########################################################################
     ###### 32 bit specific ######
    'base_name': 'WINNT_5.2_%(branch)s',
    'platform': 'win32',
    'stage_platform': 'win32',
    'publish_nightly_en_US_routes': True,
    'env': {
        'BINSCOPE': os.path.join(
            os.environ['ProgramFiles(x86)'], 'Microsoft', 'SDL BinScope', 'BinScope.exe'
        ),
        'HG_SHARE_BASE_DIR': os.path.join('y:', os.sep, 'hg-shared'),
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-firefox',
        'PDBSTR_PATH': 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x86/srcsrv/pdbstr.exe',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': 'c:/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
        'MSYSTEM': 'MINGW32',
    },
    'upload_env': {
        'UPLOAD_HOST': 'localhost',
        'UPLOAD_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s\\breakpad\\win32\\minidump_stackwalk.exe',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s\\minidumps',
    },
    'src_mozconfig': 'browser\\config\\mozconfigs\\win32\\nightly',
    #########################################################################
}
