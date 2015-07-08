# This is a template config file for marionette production.
import os
import platform

HG_SHARE_BASE_DIR = "/builds/hg-shared"

if platform.system().lower() == 'darwin':
    xre_url = "https://api.pub.build.mozilla.org/tooltool/sha512/4d8d7a37d90c34a2a2fda3066a8fe85c189b183d05389cb957fc6fed31f10a6924e50c1b84488ff61c015293803f58a3aed5d4819374d04c8e0ee2b9e3997278"
else:
    xre_url = "https://api.pub.build.mozilla.org/tooltool/sha512/dc9503b21c87b5a469118746f99e4f41d73888972ce735fa10a80f6d218086da0e3da525d9a4cd8e4ea497ec199fef720e4a525873d77a1af304ac505e076462"

config = {
    # marionette options
    "test_type": "b2g",
    "marionette_address": "localhost:2828",
    "gaiatest": True,
    "xre_url": xre_url,
    "application": "b2g",

    "vcs_share_base": HG_SHARE_BASE_DIR,
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
        'pull',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-marionette',
    ],
    "download_symbols": "ondemand",
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "vcs_output_timeout": 1760,
    "in_tree_config": "config/mozharness/marionette.py",
    "tooltool_cache": "/builds/tooltool_cache",
}
