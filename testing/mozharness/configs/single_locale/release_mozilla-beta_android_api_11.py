BRANCH = "mozilla-beta"
MOZ_UPDATE_CHANNEL = "beta"
MOZILLA_DIR = BRANCH
OBJDIR = "obj-l10n"
EN_US_BINARY_URL = "http://archive.mozilla.org/pub/mobile/candidates/%(version)s-candidates/build%(buildnum)d/android-api-11/en-US"
HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    "stage_product": "mobile",
    "log_name": "single_locale",
    "objdir": OBJDIR,
    "is_automation": True,
    "buildbot_json_path": "buildprops.json",
    "purge_minsize": 10,
    "force_clobber": True,
    "clobberer_url": "https://api.pub.build.mozilla.org/clobberer/lastclobber",
    "locales_file": "buildbot-configs/mozilla/l10n-changesets_mobile-beta.json",
    "locales_dir": "mobile/android/locales",
    "locales_platform": "android",
    "ignore_locales": ["en-US"],
    "balrog_credentials_file": "oauth.txt",
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "is_release": True,
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "exes": {
        'tooltool.py': '/tools/tooltool.py',
    },
    "repos": [{
        "repo": "https://hg.mozilla.org/releases/mozilla-beta",
        "revision": "default",
        "dest": MOZILLA_DIR,
    }, {
        "repo": "https://hg.mozilla.org/build/buildbot-configs",
        "revision": "default",
        "dest": "buildbot-configs"
    }, {
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools"
    }, {
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "revision": "RELEASE_AUTOMATION"
    }],
    "hg_l10n_base": "https://hg.mozilla.org/releases/l10n/%s" % BRANCH,
    "hg_l10n_tag": "default",
    'vcs_share_base': HG_SHARE_BASE_DIR,
    "l10n_dir": MOZILLA_DIR,

    "release_config_file": "buildbot-configs/mozilla/release-fennec-mozilla-beta.py",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_PKG_VERSION": "%(version)s",
        "MOZ_OBJDIR": OBJDIR,
        "LOCALE_MERGEDIR": "%(abs_merge_dir)s/",
        "MOZ_UPDATE_CHANNEL": MOZ_UPDATE_CHANNEL,
    },
    "base_en_us_binary_url": EN_US_BINARY_URL,
    "upload_branch": "%s-android-api-11" % BRANCH,
    "ssh_key_dir": "~/.ssh",
    "base_post_upload_cmd": "post_upload.py -p mobile -n %(buildnum)s -v %(version)s --builddir android-api-11/%(locale)s --release-to-mobile-candidates-dir --nightly-dir=candidates",
    "merge_locales": True,
    "make_dirs": ['config'],
    "mozilla_dir": MOZILLA_DIR,
    "mozconfig": "%s/mobile/android/config/mozconfigs/android-api-11/l10n-release" % MOZILLA_DIR,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "key_alias": "release",
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
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
    ],
}
