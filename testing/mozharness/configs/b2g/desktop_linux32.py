import os

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # desktop_linux64.py

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
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
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
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
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
    ###### 32 bit specific ######
    'base_name': 'B2G_%(branch)s_linux32_gecko',
    'platform': 'linux32_gecko',
    'stage_platform': 'linux32_gecko',
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
        # 32 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib/ccache:\
/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:\
/tools/python27/bin:/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib",
    },
    'upload_env': {
        # stage_server is dictated from build_pool_specifics.py
        'UPLOAD_HOST': '%(stage_server)s',
        'UPLOAD_USER': '%(stage_username)s',
        'UPLOAD_SSH_KEY': '/home/mock_mozilla/.ssh/%(stage_ssh_key)s',
        'UPLOAD_TO_TEMP': '1',
    },
    'mock_packages': [
        'autoconf213', 'python', 'mozilla-python27', 'zip', 'mozilla-python27-mercurial',
        'git', 'ccache', 'perl-Test-Simple', 'perl-Config-General',
        'yasm', 'wget',
        'mpfr',  # required for system compiler
        'xorg-x11-font*',  # fonts required for PGO
        'imake',  # required for makedepend!?!
        ### <-- from releng repo
        'gcc45_0moz3', 'gcc454_0moz1', 'gcc472_0moz1', 'gcc473_0moz1',
        'yasm', 'ccache',
        ###
        'valgrind',
        ######## 32 bit specific ###########
        'glibc-static.i686', 'libstdc++-static.i686',
        'gtk2-devel.i686', 'libnotify-devel.i686',
        'alsa-lib-devel.i686', 'libcurl-devel.i686',
        'wireless-tools-devel.i686', 'libX11-devel.i686',
        'libXt-devel.i686', 'mesa-libGL-devel.i686',
        'gnome-vfs2-devel.i686', 'GConf2-devel.i686',
        'pulseaudio-libs-devel.i686',
        'gstreamer-devel.i686', 'gstreamer-plugins-base-devel.i686',
        # Packages already installed in the mock environment, as x86_64
        # packages.
        'glibc-devel.i686', 'libgcc.i686', 'libstdc++-devel.i686',
        # yum likes to install .x86_64 -devel packages that satisfy .i686
        # -devel packages dependencies. So manually install the dependencies
        # of the above packages.
        'ORBit2-devel.i686', 'atk-devel.i686', 'cairo-devel.i686',
        'check-devel.i686', 'dbus-devel.i686', 'dbus-glib-devel.i686',
        'fontconfig-devel.i686', 'glib2-devel.i686',
        'hal-devel.i686', 'libICE-devel.i686', 'libIDL-devel.i686',
        'libSM-devel.i686', 'libXau-devel.i686', 'libXcomposite-devel.i686',
        'libXcursor-devel.i686', 'libXdamage-devel.i686',
        'libXdmcp-devel.i686', 'libXext-devel.i686', 'libXfixes-devel.i686',
        'libXft-devel.i686', 'libXi-devel.i686', 'libXinerama-devel.i686',
        'libXrandr-devel.i686', 'libXrender-devel.i686',
        'libXxf86vm-devel.i686', 'libdrm-devel.i686', 'libidn-devel.i686',
        'libpng-devel.i686', 'libxcb-devel.i686', 'libxml2-devel.i686',
        'pango-devel.i686', 'perl-devel.i686', 'pixman-devel.i686',
        'zlib-devel.i686',
        # Freetype packages need to be installed be version, because a newer
        # version is available, but we don't want it for Firefox builds.
        'freetype-2.3.11-6.el6_1.8.i686',
        'freetype-devel-2.3.11-6.el6_1.8.i686',
        'freetype-2.3.11-6.el6_1.8.x86_64',
        ######## 32 bit specific ###########
        # The B2G SDK is still 64-bit, and we need libXt for that
        'libXt.x86_64',
    ],
    'src_mozconfig': 'b2g/config/mozconfigs/linux32_gecko/nightly',
    'tooltool_manifest_src': "b2g/config/tooltool-manifests/linux32/releng.manifest",
    #########################################################################
}
