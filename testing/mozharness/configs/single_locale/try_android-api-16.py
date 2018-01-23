import os

BRANCH = "try"
MOZILLA_DIR = BRANCH
EN_US_BINARY_URL = "http://archive.mozilla.org/pub/" \
                   "mobile/nightly/latest-mozilla-central-android-api-16/en-US"

config = {
    "branch": "try",
    "log_name": "single_locale",
    "objdir": "obj-firefox",
    "is_automation": True,
    "buildbot_json_path": "buildprops.json",
    "force_clobber": True,
    "clobberer_url": "https://api.pub.build.mozilla.org/clobberer/lastclobber",
    "locales_file": "%s/mobile/locales/l10n-changesets.json" % MOZILLA_DIR,
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    'balrog_credentials_file': 'oauth.txt',
    "tools_repo": "https://hg.mozilla.org/build/tools",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/" + MOZILLA_DIR,
    },
    "exes": {
        'tooltool.py': '/builds/tooltool.py',
    },
    "update_gecko_source_to_enUS": False,
    "nightly_build": True,
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/try",
        "revision": "%(revision)s",
        "dest": "try",
        "clone_upstream_url": "https://hg.mozilla.org/mozilla-unified",
        "clone_by_revision": True,
        "clone_with_purge": True,
    }],
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",
    "hg_l10n_tag": "default",
    'vcs_share_base': "/builds/hg-shared",

    "l10n_dir": "l10n-central",
    "repack_env": {
        # so ugly, bug 951238
        "LD_LIBRARY_PATH": "/lib:/tools/gcc-4.7.2-0moz1/lib:/tools/gcc-4.7.2-0moz1/lib64",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ.get("EN_US_BINARY_URL", EN_US_BINARY_URL),
        "MOZ_UPDATE_CHANNEL": "try", # XXX Invalid
    },
    "upload_branch": "%s-android-api-16" % BRANCH,
    "ssh_key_dir": "~/.ssh",
    "mozilla_dir": MOZILLA_DIR,
    "mozconfig": "%s/mobile/android/config/mozconfigs/android-api-16/l10n-nightly" % MOZILLA_DIR,
    "signature_verification_script": "tools/release/signing/verify-android-signature.sh",
    "stage_product": "mobile",
    "platform": "android", # XXX Validate
    "build_type": "api-16-opt", # XXX Validate

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
