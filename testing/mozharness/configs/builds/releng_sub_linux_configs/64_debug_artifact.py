import os

MOZ_OBJDIR = 'obj-firefox'

config = {
    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'clobber',
        'clone-tools',
        'checkout-sources',
        'setup-mock',
        'build',
        'sendchange',
        # 'generate-build-stats',
    ],
    "buildbot_json_path": "buildprops.json",
    'exes': {
        "buildbot": "/tools/buildbot/bin/buildbot",
    },
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    'enable_ccache': True,
    'vcs_share_base': '/builds/hg-shared',
    'objdir': MOZ_OBJDIR,
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'enable_count_ctors': True,
    # debug specific
    'debug_build': True,
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': False,
    'enable_talos_sendchange': False,
    # allows triggering of test jobs when --artifact try syntax is detected on buildbot
    'enable_unittest_sendchange': True,
    'perfherder_extra_options': ['artifact'],
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'base_name': 'Linux_x86-64_%(branch)s_Artifact_build',
    'platform': 'linux64',
    'stage_platform': 'linux64-debug',
    'publish_nightly_en_US_routes': False,
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_AUTOMATION': '1',
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
        ## 64 bit specific
        'PATH': '/tools/buildbot/bin:/usr/local/bin:/usr/lib64/ccache:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/tools/git/bin:/tools/python27/bin:\
/tools/python27-mercurial/bin:/home/cltbld/bin',
        'LD_LIBRARY_PATH': "/tools/gcc-4.3.3/installed/lib64",
        ##
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
        'valgrind', 'dbus-x11',
        ######## 64 bit specific ###########
        'glibc-static', 'libstdc++-static',
        'gtk2-devel', 'libnotify-devel',
        'alsa-lib-devel', 'libcurl-devel', 'wireless-tools-devel',
        'libX11-devel', 'libXt-devel', 'mesa-libGL-devel', 'gnome-vfs2-devel',
        'GConf2-devel',
        ### from releng repo
        'gcc45_0moz3', 'gcc454_0moz1', 'gcc472_0moz1', 'gcc473_0moz1',
        'yasm', 'ccache',
        ###
        'pulseaudio-libs-devel', 'gstreamer-devel',
        'gstreamer-plugins-base-devel', 'freetype-2.3.11-6.el6_1.8.x86_64',
        'freetype-devel-2.3.11-6.el6_1.8.x86_64'
    ],
    'src_mozconfig': 'browser/config/mozconfigs/linux64/debug-artifact',
    'tooltool_manifest_src': "browser/config/tooltool-manifests/linux64/\
releng.manifest",
    #######################
}
