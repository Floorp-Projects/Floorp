import os

config = {
    "platform": "linux64",
    "stage_product": "firefox",
    "update_platform": "Linux_x86_64-gcc3",
    "mozconfig": "%(branch)s/browser/config/mozconfigs/linux64/l10n-mozconfig",
    "bootstrap_env": {
        "MOZ_OBJDIR": "obj-l10n",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
    },
    "ssh_key_dir": "/home/mock_mozilla/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-l10n",
    "js_src_dir": "js/src",
    "vcs_share_base": "/builds/hg-shared",

    # tooltool
    'tooltool_url': 'https://api.pub.build.mozilla.org/tooltool/',
    'tooltool_script': ["/builds/tooltool.py"],
    'tooltool_bootstrap': "setup.sh",
    'tooltool_manifest_src': 'browser/config/tooltool-manifests/linux64/releng.manifest',
    # balrog credential file:
    'balrog_credentials_file': 'oauth.txt',

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
    "locales_file": "%(branch)s/browser/locales/all-locales",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",
    "merge_locales": True,

    # MAR
    "previous_mar_dir": "dist/previous",
    "current_mar_dir": "dist/current",
    "update_mar_dir": "dist/update",  # sure?
    "previous_mar_filename": "previous.mar",
    "current_work_mar_dir": "current.work",
    "package_base_dir": "dist/l10n-stage",
    "application_ini": "application.ini",
    "buildid_section": 'App',
    "buildid_option": "BuildID",
    "unpack_script": "tools/update-packaging/unwrap_full_update.pl",
    "incremental_update_script": "tools/update-packaging/make_incremental_update.sh",
    "balrog_release_pusher_script": "scripts/updates/balrog-release-pusher.py",
    "update_packaging_dir": "tools/update-packaging",
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",
    "current_mar_filename": "firefox-%(version)s.%(locale)s.linux-x86_64.complete.mar",
    "complete_mar": "firefox-%(version)s.en-US.linux-x86_64.complete.mar",
    "localized_mar": "firefox-%(version)s.%(locale)s.linux-x86_64.complete.mar",
    "partial_mar": "firefox-%(version)s.%(locale)s.linux-x86_64.partial.%(from_buildid)s-%(to_buildid)s.mar",
    "installer_file": "firefox-%(version)s.en-US.linux-x86_64.tar.bz2",

    # Mock
    'mock_target': 'mozilla-centos6-x86_64',

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
    'mock_files': [
        ('/home/cltbld/.ssh', '/home/mock_mozilla/.ssh'),
        ('/home/cltbld/.hgrc', '/builds/.hgrc'),
        ('/home/cltbld/.boto', '/builds/.boto'),
        ('/builds/gapi.data', '/builds/gapi.data'),
        ('/builds/relengapi.tok', '/builds/relengapi.tok'),
        ('/tools/tooltool.py', '/builds/tooltool.py'),
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
    ],
}
