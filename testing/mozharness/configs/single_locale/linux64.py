import os

config = {
    "platform": "linux64",
    "stage_product": "firefox",
    "mozconfig": "src/browser/config/mozconfigs/linux64/l10n-mozconfig",
    "bootstrap_env": {
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": "%(en_us_binary_url)s",
        "MOZ_UPDATE_CHANNEL": "%(update_channel)s",
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        "MOZ_MAKE_COMPLETE_MAR": "1",
        'EN_US_PACKAGE_NAME': 'target.tar.bz2',
    },
    "ssh_key_dir": "/home/mock_mozilla/.ssh",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "vcs_share_base": "/builds/hg-shared",

    # balrog credential file:
    'balrog_credentials_file': 'oauth.txt',

    # l10n
    "ignore_locales": ["en-US", "ja-JP-mac"],
    "l10n_dir": "l10n",
    "locales_file": "src/browser/locales/all-locales",
    "locales_dir": "browser/locales",
    "hg_l10n_tag": "default",

    # MAR
    "update_mar_dir": "dist/update",  # sure?
    "application_ini": "application.ini",
    "local_mar_tool_dir": "dist/host/bin",
    "mar": "mar",
    "mbsdiff": "mbsdiff",
    "localized_mar": "firefox-%(version)s.%(locale)s.linux-x86_64.complete.mar",

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
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
    ],
}
