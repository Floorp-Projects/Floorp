# This config file is for locally testing spidermonkey_build.py. It provides
# the values that would otherwise be provided by buildbot.

BRANCH = "local-src"
HOME = "/home/sfink"
REPO = HOME + "/src/MI-GC"

config = {
    "hgurl": "https://hg.mozilla.org/",
    "hgtool_base_bundle_urls": [
        "https://ftp-ssl.mozilla.org/pub/mozilla.org/firefox/bundles"
    ],

    "python": "python",
    "sixgill": HOME + "/src/sixgill",
    "sixgill_bin": HOME + "/src/sixgill/bin",

    "repo": REPO,
    "repos": [{
        "repo": REPO,
        "revision": "default",
        "dest": BRANCH,
    }, {
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools"
    }],

    "tools_dir": "/tools",

    "mock_target": "mozilla-centos6-x86_64",

    "upload_remote_basepath": "/tmp/upload-base",
    "upload_ssh_server": "localhost",
    "upload_ssh_key": "/home/sfink/.ssh/id_rsa",
    "upload_ssh_user": "sfink",
    "upload_label": "linux64-br-haz",

    # For testing tryserver uploads (directory structure is different)
    #"branch": "try",
    #"revision": "deadbeef1234",
}
