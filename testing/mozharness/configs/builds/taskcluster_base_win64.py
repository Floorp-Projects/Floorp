import os
import sys

config = {
    'default_actions': [
        'clone-tools',
        'build',
        'check-test',
    ],
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'vcs_share_base': os.path.join('y:', os.sep, 'hg-shared'),
    'tooltool_script': [
      sys.executable,
      os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')
    ],
    'tooltool_bootstrap': 'setup.sh',
    'enable_count_ctors': False,
    'max_build_output_timeout': 60 * 80,


    ###### 64 bit specific ######
    'base_name': 'WINNT_6.1_x86-64_%(branch)s',
    'platform': 'win64',
    'publish_nightly_en_US_routes': True,
    'env': {
        'HG_SHARE_BASE_DIR': os.path.join('y:', os.sep, 'hg-shared'),
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'PDBSTR_PATH': 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/srcsrv/pdbstr.exe',
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
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s\\breakpad\\win64\\minidump_stackwalk.exe',
        'MINIDUMP_SAVE_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },
    'mozconfig_platform': 'win64',
}
