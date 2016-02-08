# This is a template config file for b2g emulator unittest testing
import platform
import os

HG_SHARE_BASE_DIR = "/builds/hg-shared"

config = {
    # mozharness script options
    "vcs_share_base": HG_SHARE_BASE_DIR,

    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],
    "download_symbols": "ondemand",
    # We bake this directly into the tester image now.
    "download_minidump_stackwalk": False,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "vcs_output_timeout": 1760,
}
