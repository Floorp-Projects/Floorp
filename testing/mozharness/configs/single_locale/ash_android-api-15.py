BRANCH = "ash"
MOZ_UPDATE_CHANNEL = "nightly"
MOZILLA_DIR = BRANCH
OBJDIR = "obj-l10n"
EN_US_BINARY_URL = "http://archive.mozilla.org/pub/mobile/nightly/latest-%s-android-api-15/en-US" % BRANCH
HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    "branch": BRANCH,
    "log_name": "single_locale",
    "objdir": OBJDIR,
    "is_automation": True,
    "buildbot_json_path": "buildprops.json",
    "force_clobber": True,
    "clobberer_url": "https://api.pub.build.mozilla.org/clobberer/lastclobber",
    "locales_file": "%s/mobile/android/locales/all-locales" % MOZILLA_DIR,
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    "nightly_build": True,
    'balrog_credentials_file': 'oauth.txt',
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "exes": {
        'tooltool.py': '/builds/tooltool.py',
    },
    "repos": [{
        "repo": "https://hg.mozilla.org/projects/ash",
        "branch": "default",
        "dest": MOZILLA_DIR,
    }, {
        "repo": "https://hg.mozilla.org/build/buildbot-configs",
        "branch": "default",
        "dest": "buildbot-configs"
    }, {
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools"
    }],
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': HG_SHARE_BASE_DIR,

    "l10n_dir": "l10n-central",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_OBJDIR": OBJDIR,
        "EN_US_BINARY_URL": EN_US_BINARY_URL,
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
        "MOZ_UPDATE_CHANNEL": MOZ_UPDATE_CHANNEL,
    },
    "upload_branch": "%s-android-api-15" % BRANCH,
    "ssh_key_dir": "~/.ssh",
    "merge_locales": True,
    "mozilla_dir": MOZILLA_DIR,
    "mozconfig": "%s/mobile/android/config/mozconfigs/android-api-15/l10n-nightly" % MOZILLA_DIR,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "stage_product": "mobile",
    "platform": "android",
    "build_type": "api-15-opt",

    # Balrog
    "build_target": "Android_arm-eabi-gcc3",

    # Mock
    "mock_target": "mozilla-centos6-x86_64-android",
    "mock_packages": ['autoconf213', 'python', 'zip', 'mozilla-python27-mercurial', 'git', 'ccache',
                      'glibc-static', 'libstdc++-static', 'perl-Test-Simple', 'perl-Config-General',
                      'gtk2-devel', 'libnotify-devel', 'yasm',
                      'alsa-lib-devel', 'libcurl-devel',
                      'wireless-tools-devel', 'libX11-devel',
                      'libXt-devel', 'mesa-libGL-devel',
                      'gnome-vfs2-devel', 'GConf2-devel', 'wget',
                      'mpfr',  # required for system compiler
                      'xorg-x11-font*',  # fonts required for PGO
                      'imake',  # required for makedepend!?!
                      'gcc45_0moz3', 'gcc454_0moz1', 'gcc472_0moz1', 'gcc473_0moz1', 'yasm', 'ccache',  # <-- from releng repo
                      'valgrind', 'dbus-x11',
                      'pulseaudio-libs-devel',
                      'gstreamer-devel', 'gstreamer-plugins-base-devel',
                      'freetype-2.3.11-6.el6_1.8.x86_64',
                      'freetype-devel-2.3.11-6.el6_1.8.x86_64',
                      'java-1.7.0-openjdk-devel',
                      'openssh-clients',
                      'zlib-devel-1.2.3-27.el6.i686',
                      ],
    "mock_files": [
        ("/home/cltbld/.ssh", "/home/mock_mozilla/.ssh"),
        ('/home/cltbld/.hgrc', '/builds/.hgrc'),
        ('/builds/relengapi.tok', '/builds/relengapi.tok'),
        ('/tools/tooltool.py', '/builds/tooltool.py'),
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
    ],
}
