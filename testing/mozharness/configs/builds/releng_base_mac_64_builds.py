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
        'upload-files',
        'sendchange',
        'check-test',
        'generate-build-stats',
        'update',  # decided by query_is_nightly()
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'python2.7': sys.executable,
        'hgtool.py': os.path.join(
            os.getcwd(), 'build', 'tools', 'buildfarm', 'utils', 'hgtool.py'
        ),
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'purge_skip': ['info', 'rel-*:45d', 'tb-rel-*:45d'],
    'purge_basedirs':  [],
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': 'obj-firefox/i386',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': False,
    'enable_talos_sendchange': True,
    'enable_unittest_sendchange': True,
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'OS X 10.7 %(branch)s',
    'platform': 'macosx64',
    'stage_platform': 'macosx64',
    'use_platform_in_symbols_extra_buildid': True,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-firefox',
        # SYMBOL_SERVER_HOST is dictated from build_pool_specifics.py
        'SYMBOL_SERVER_HOST': '%(symbol_server_host)s',
        'SYMBOL_SERVER_SSH_KEY': "/Users/cltbld/.ssh/ffxbld_rsa",
        'SYMBOL_SERVER_USER': 'ffxbld',
        'SYMBOL_SERVER_PATH': '/mnt/netapp/breakpad/symbols_ffx/',
        'POST_SYMBOL_UPLOAD_CMD': '/usr/local/bin/post-symbol-upload.py',
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
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/Users/cltbld/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s/breakpad/osx64/minidump_stackwalk',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s/minidumps',
    },
    'purge_minsize': 12,
    'src_mozconfig': 'browser/config/mozconfigs/macosx-universal/nightly',
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/macosx64/releng.manifest',
    #########################################################################
}
