HG_SHARE_BASE_DIR = "/builds/hg-shared"

PYTHON_DIR = "/tools/python27"
SRCDIR = "source"

config = {
    "log_name": "hazards",
    "shell-objdir": "obj-opt-js",
    "analysis-dir": "analysis",
    "analysis-objdir": "obj-analyzed",
    "srcdir": SRCDIR,
    "analysis-scriptdir": "js/src/devtools/rootAnalysis",

    # These paths are relative to the tooltool checkout location
    "sixgill": "sixgill/usr/libexec/sixgill",
    "sixgill_bin": "sixgill/usr/bin",

    "python": "python",

    "exes": {
        'hgtool.py': '%(abs_tools_dir)s/buildfarm/utils/hgtool.py',
        'gittool.py': '%(abs_tools_dir)s/buildfarm/utils/gittool.py',
        'tooltool.py': '/tools/tooltool.py',
    },

    "purge_minsize": 18,
    "force_clobber": True,
    'vcs_share_base': HG_SHARE_BASE_DIR,

    "repos": [{
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools"
    }],

    "upload_remote_baseuri": 'https://ftp-ssl.mozilla.org/',

    'tools_dir': "/tools",
    'compiler_manifest': "build/gcc.manifest",
    'b2g_compiler_manifest': "build/gcc-b2g.manifest",
    'compiler_setup': "setup.sh.gcc",
    'sixgill_manifest': "build/sixgill.manifest",
    'sixgill_setup': "setup.sh.sixgill",

    # Mock.
    "mock_packages": [
        "autoconf213", "mozilla-python27-mercurial", "ccache",
        "zip", "zlib-devel", "glibc-static",
        "openssh-clients", "mpfr", "wget", "rsync",

        # For building the JS shell
        "gmp-devel", "nspr", "nspr-devel",

        # For building the browser
        "dbus-devel", "dbus-glib-devel", "hal-devel",
        "libICE-devel", "libIDL-devel",

        # For mach resource-usage
        "python-psutil",

        'zip', 'git',
        'libstdc++-static', 'perl-Test-Simple', 'perl-Config-General',
        'gtk2-devel', 'libnotify-devel', 'yasm',
        'alsa-lib-devel', 'libcurl-devel',
        'wireless-tools-devel', 'libX11-devel',
        'libXt-devel', 'mesa-libGL-devel',
        'gnome-vfs2-devel', 'GConf2-devel', 'wget',
        'mpfr',  # required for system compiler
        'xorg-x11-font*',  # fonts required for PGO
        'imake',  # required for makedepend!?!
        'pulseaudio-libs-devel',
        'freetype-2.3.11-6.el6_1.8.x86_64',
        'freetype-devel-2.3.11-6.el6_1.8.x86_64',
        'gstreamer-devel', 'gstreamer-plugins-base-devel',
    ],
    "mock_files": [
        ("/home/cltbld/.ssh", "/home/mock_mozilla/.ssh"),
        ("/tools/tooltool.py", "/tools/tooltool.py"),
    ],
    "env_replacements": {
        "pythondir": PYTHON_DIR,
        "gccdir": "%(abs_work_dir)s/gcc",
        "sixgilldir": "%(abs_work_dir)s/sixgill",
    },
    "partial_env": {
        "PATH": "%(pythondir)s/bin:%(gccdir)s/bin:%(PATH)s",
        "LD_LIBRARY_PATH": "%(sixgilldir)s/usr/lib64",
    },
}
