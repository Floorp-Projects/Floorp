# This is a template config file for b2g desktop unittest production.
import os

config = {
    # mozharness options
    "application": "b2g",

    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

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

    "run_file_names": {
        "mochitest": "runtestsb2g.py",
        "reftest": "runreftestb2g.py",
    },
   "in_tree_config": "config/mozharness/b2g_desktop_config.py",
}
