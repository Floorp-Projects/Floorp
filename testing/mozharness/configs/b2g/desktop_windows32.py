import os
import sys

config = {
    #########################################################################
    ######## WINDOWS GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_windows_64_builds.py

    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'build',
        'upload-files',
        'sendchange',
        'check-test',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'python2.7': sys.executable,
        'gittool.py': [
            sys.executable,
            os.path.join(
                os.getcwd(), 'build', 'tools', 'buildfarm', 'utils', 'gittool.py'
            )
        ],
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
    'enable_ccache': False,
    'vcs_share_base': 'C:\\builds\\hg-shared',
    'objdir': 'obj-firefox',
    'tooltool_script': [sys.executable,
                        'C:/mozilla-build/tooltool.py'],
    'tooltool_bootstrap': "setup.sh",
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
    #########################################################################


     #########################################################################
     ###### 32 bit specific ######
    'base_name': 'B2G_%(branch)s_win32_gecko',
    'platform': 'win32_gecko',
    'stage_platform': 'win32_gecko',
    'stage_product': 'b2g',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'BINSCOPE': 'C:/Program Files (x86)/Microsoft/SDL BinScope/BinScope.exe',
        'HG_SHARE_BASE_DIR': 'C:/builds/hg-shared',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': 'obj-firefox',
        'PATH': 'C:/mozilla-build/nsis-3.0b1;C:/mozilla-build/python27;'
                'C:/mozilla-build/buildbotve/scripts;'
                '%s' % (os.environ.get('path')),
        'PDBSTR_PATH': '/c/Program Files (x86)/Windows Kits/8.0/Debuggers/x64/srcsrv/pdbstr.exe',
        'PROPERTIES_FILE': os.path.join(os.getcwd(), 'buildprops.json'),
        'TINDERBOX_OUTPUT': '1',
    },
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/c/Users/cltbld/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s/breakpad/win32/minidump_stackwalk.exe',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s/minidumps',
    },
    'enable_pymake': True,
    'src_mozconfig': 'b2g/config/mozconfigs/win32_gecko/nightly',
    'tooltool_manifest_src': "b2g/config/tooltool-manifests/win32/releng.manifest",
    #########################################################################
}
