import os

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 64 bit keys/values please add them
    # below under the '64 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # desktop_linux32.py

    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'build',
        'upload-files',
        'sendchange',
        'check-test',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        'hgtool.py': os.path.join(
            os.getcwd(), 'build', 'tools', 'buildfarm', 'utils', 'hgtool.py'
        ),
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    'purge_skip': ['info', 'rel-*:45d', 'tb-rel-*:45d'],
    'purge_basedirs':  ["/mock/users/cltbld/home/cltbld/build"],
    # mock shtuff
    'mock_mozilla_dir':  '/builds/mock_mozilla',
    'mock_target': 'mozilla-centos6-x86_64',
    'mock_files': [
        ('/home/cltbld/.ssh', '/home/mock_mozilla/.ssh'),
        ('/home/cltbld/.hgrc', '/builds/.hgrc'),
        ('/home/cltbld/.boto', '/builds/.boto'),
        ('/builds/gapi.data', '/builds/gapi.data'),
        ('/builds/relengapi.tok', '/builds/relengapi.tok'),
        ('/tools/tooltool.py', '/builds/tooltool.py'),
        ('/builds/mozilla-desktop-geoloc-api.key', '/builds/mozilla-desktop-geoloc-api.key'),
        ('/builds/crash-stats-api.token', '/builds/crash-stats-api.token'),
    ],
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': 'obj-firefox',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': True,
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'B2G_%(branch)s_linux64_gecko',
    'platform': 'linux64_gecko',
    'stage_platform': 'linux64_gecko',
    'stage_product': 'b2g',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
        'DISPLAY': ':2',
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
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib64",
        ##
    },
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/home/mock_mozilla/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    'purge_minsize': 14,
    'mock_packages': ['autoconf213', 'mozilla-python27', 'zip',
                      'mozilla-python27-mercurial', 'git', 'ccache',
                      'glibc-static', 'libstdc++-static', 'perl-Test-Simple',
                      'perl-Config-General', 'gtk2-devel', 'libnotify-devel',
                      'yasm', 'alsa-lib-devel', 'libcurl-devel',
                      'wireless-tools-devel', 'libX11-devel', 'libXt-devel',
                      'mesa-libGL-devel', 'gnome-vfs2-devel', 'mpfr',
                      'xorg-x11-font', 'imake', 'ccache', 'wget',
                      'freetype-2.3.11-6.el6_2.9',
                      'freetype-devel-2.3.11-6.el6_2.9', 'gstreamer-devel',
                      'gstreamer-plugins-base-devel'],
    'src_mozconfig': 'b2g/config/mozconfigs/linux64_gecko/nightly',
    'tooltool_manifest_src': "b2g/config/tooltool-manifests/linux64/releng.manifest",
    #########################################################################
}
