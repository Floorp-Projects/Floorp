import os

MOZ_OBJDIR = 'obj-firefox'

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_linux_64_builds.py

    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'build',
        'sendchange',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': False,
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': MOZ_OBJDIR,
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': True,
    # debug specific
    'debug_build': True,
    'enable_talos_sendchange': False,
    # allows triggering of test jobs when --artifact try syntax is detected on buildbot
    'enable_unittest_sendchange': True,
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 32 bit specific ######
    'base_name': 'Linux_%(branch)s_Artifact_build',
    'platform': 'linux',
    'stage_platform': 'linux-debug',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
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
        # 32 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib/ccache:\
/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:\
/tools/python27/bin:/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib",
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
    ],
    'src_mozconfig': 'browser/config/mozconfigs/linux32/debug-artifact',
    'tooltool_manifest_src': "browser/config/tooltool-manifests/linux32/\
releng.manifest",
    #########################################################################
}
