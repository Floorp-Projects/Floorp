import os

HG_SHARE_BASE_DIR = "/builds/hg-shared"

PYTHON_DIR = "/tools/python27"
SRCDIR = "source"

config = {
    "platform": "linux64",
    "build_type": "br-haz",
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
        'gittool.py': '%(abs_tools_dir)s/buildfarm/utils/gittool.py',
        'tooltool.py': '/tools/tooltool.py',
        "virtualenv": [PYTHON_DIR + "/bin/python", "/tools/misc-python/virtualenv.py"],
    },

    "force_clobber": True,
    'vcs_share_base': HG_SHARE_BASE_DIR,

    "repos": [{
        "repo": "https://hg.mozilla.org/build/tools",
        "branch": "default",
        "dest": "tools"
    }],

    "upload_remote_baseuri": 'https://ftp-ssl.mozilla.org/',
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),

    "virtualenv_path": '%s/venv' % os.getcwd(),
    'tools_dir': "/tools",
    'compiler_manifest': "build/gcc.manifest",
    'b2g_compiler_manifest': "build/gcc-b2g.manifest",
    'sixgill_manifest': "build/sixgill.manifest",

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
        ('/home/cltbld/.hgrc', '/builds/.hgrc'),
        ('/builds/relengapi.tok', '/builds/relengapi.tok'),
        ("/tools/tooltool.py", "/tools/tooltool.py"),
        ('/usr/local/lib/hgext', '/usr/local/lib/hgext'),
    ],
    "env_replacements": {
        "pythondir": PYTHON_DIR,
        "gccdir": "%(abs_work_dir)s/gcc",
        "sixgilldir": "%(abs_work_dir)s/sixgill",
    },
    "partial_env": {
        "PATH": "%(pythondir)s/bin:%(gccdir)s/bin:%(PATH)s",
        "LD_LIBRARY_PATH": "%(sixgilldir)s/usr/lib64",

        # Suppress the mercurial-setup check. When running in automation, this
        # is redundant with MOZ_AUTOMATION, but a local developer-mode build
        # will have the mach state directory set to a nonstandard location and
        # therefore will always claim that mercurial-setup has not been run.
        "I_PREFER_A_SUBOPTIMAL_MERCURIAL_EXPERIENCE": "1",
    },
}
