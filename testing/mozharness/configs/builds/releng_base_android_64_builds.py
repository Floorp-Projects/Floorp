import os

config = {
    #########################################################################
    ######## ANDROID GENERIC CONFIG KEYS/VAlUES

    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'build',
        'upload-files',
        'sendchange',
        'multi-l10n',
        'update',  # decided by query_is_nightly()
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'hgtool.py': os.path.join(
            os.getcwd(), 'build', 'tools', 'buildfarm', 'utils', 'hgtool.py'
        ),
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'purge_skip': ['info', 'rel-*:45d', 'tb-rel-*:45d'],
    'purge_basedirs':  ["/mock/users/cltbld/home/cltbld/build"],
    # mock shtuff
    'mock_mozilla_dir':  '/builds/mock_mozilla',
    'mock_target': 'mozilla-centos6-x86_64-android',
    'mock_files': [
        ('/home/cltbld/.ssh', '/home/mock_mozilla/.ssh'),
        ('/home/cltbld/.hgrc', '/builds/.hgrc'),
        ('/home/cltbld/.boto', '/builds/.boto'),
        ('/builds/mozilla-api.key', '/builds/mozilla-api.key'),
        ('/builds/mozilla-fennec-geoloc-api.key', '/builds/mozilla-fennec-geoloc-api.key'),
        ('/builds/crash-stats-api.token', '/builds/crash-stats-api.token'),
    ],
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': 'obj-firefox',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': False,
    'enable_talos_sendchange': True,
    'enable_unittest_sendchange': True,
    'multi_locale': True,
    #########################################################################


    #########################################################################
    'base_name': 'Android 2.3 %(branch)s',
    'platform': 'android',
    'stage_platform': 'android',
    'stage_product': 'mobile',
    'post_upload_include_platform': True,
    'enable_max_vsize': False,
    'use_package_as_marfile': True,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': 'obj-firefox',
        # SYMBOL_SERVER_HOST is dictated from build_pool_specifics.py
        'SYMBOL_SERVER_HOST': '%(symbol_server_host)s',
        'SYMBOL_SERVER_SSH_KEY': "/home/mock_mozilla/.ssh/ffxbld_rsa",
        'SYMBOL_SERVER_USER': 'ffxbld',
        'SYMBOL_SERVER_PATH': '/mnt/netapp/breakpad/symbols_ffx/',
        'POST_SYMBOL_UPLOAD_CMD': '/usr/local/bin/post-symbol-upload.py',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'CCACHE_DIR': '/builds/ccache',
        'CCACHE_COMPRESS': '1',
        'CCACHE_UMASK': '002',
        'LC_ALL': 'C',
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/bin:/usr/bin',
        'SHIP_LICENSED_FONTS': '1',
    },
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/home/mock_mozilla/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    "check_test_env": {
        'MINIDUMP_STACKWALK': '%(abs_tools_dir)s/breakpad/linux/minidump_stackwalk',
        'MINIDUMP_SAVE_PATH': '%(base_work_dir)s/minidumps',
    },
    'purge_minsize': 12,
    'mock_packages': ['autoconf213', 'mozilla-python27-mercurial', 'yasm',
                      'ccache', 'zip', "gcc472_0moz1", "gcc473_0moz1",
                      'java-1.7.0-openjdk-devel', 'zlib-devel',
                      'glibc-static', 'openssh-clients', 'mpfr',
                      'wget', 'glibc.i686', 'libstdc++.i686',
                      'zlib.i686', 'freetype-2.3.11-6.el6_1.8.x86_64',
                      'ant', 'ant-apache-regexp'
                      ],
    'src_mozconfig': 'mobile/android/config/mozconfigs/android/nightly',
    'tooltool_manifest_src': "mobile/android/config/tooltool-manifests/android/releng.manifest",
    #########################################################################
}
