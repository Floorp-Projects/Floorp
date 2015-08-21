# This is a template config file for b2g emulator unittest production.
import os

config = {
    # mozharness options
    "application": "b2g",
    "busybox_url": "https://api.pub.build.mozilla.org/tooltool/sha512/0748e900821820f1a42e2f1f3fa4d9002ef257c351b9e6b78e7de0ddd0202eace351f440372fbb1ae0b7e69e8361b036f6bd3362df99e67fc585082a311fc0df",
    "xre_url": "https://api.pub.build.mozilla.org/tooltool/sha512/dc9503b21c87b5a469118746f99e4f41d73888972ce735fa10a80f6d218086da0e3da525d9a4cd8e4ea497ec199fef720e4a525873d77a1af304ac505e076462",
    "tooltool_cache": "/builds/tooltool_cache",

    "exes": {
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
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
        "mochitest-chrome": "runtestsb2g.py",
        "reftest": "runreftestb2g.py",
        "crashtest": "runreftestb2g.py",
        "xpcshell": "runtestsb2g.py",
        "cppunittest": "remotecppunittests.py"
    },
    # test harness options are located in the gecko tree
    "in_tree_config": "config/mozharness/b2g_emulator_config.py",
    "vcs_output_timeout": 1760,
}
