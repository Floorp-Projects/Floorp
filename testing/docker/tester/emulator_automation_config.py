# This is a template config file for b2g emulator unittest production.
import os

config = {
    # mozharness options
    "application": "b2g",
    "busybox_url": "http://runtime-binaries.pvt.build.mozilla.org/tooltool/sha512/0748e900821820f1a42e2f1f3fa4d9002ef257c351b9e6b78e7de0ddd0202eace351f440372fbb1ae0b7e69e8361b036f6bd3362df99e67fc585082a311fc0df",
    "xre_url": "http://runtime-binaries.pvt.build.mozilla.org/tooltool/sha512/263f4e8796c25543f64ba36e53d5c4ab8ed4d4e919226037ac0988761d34791b038ce96a8ae434f0153f9c2061204086decdbff18bdced42f3849156ae4dc9a4",
    "tooltool_servers": ["http://runtime-binaries.pvt.build.mozilla.org/tooltool/"],

    "exes": {
        'python': '/usr/bin/python',
        'tooltool.py': "mozharness/mozharness/mozilla/tooltool.py",
    },

    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "buildbot_json_path": "buildprops.json",

    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],
    "download_symbols": "ondemand",
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),

    "run_file_names": {
        "jsreftest": "runreftestb2g.py",
        "mochitest": "runtestsb2g.py",
        "reftest": "runreftestb2g.py",
        "crashtest": "runreftestb2g.py",
        "xpcshell": "runtestsb2g.py"
    },
    # test harness options are located in the gecko tree
    "in_tree_config": "config/mozharness/b2g_emulator_config.py",
    "vcs_output_timeout": 1760,
}
