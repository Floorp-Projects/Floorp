# This is a template config file for mulet unittest testing
import os

config = {
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "download_symbols": "ondemand",
    # We bake this directly into the tester image now.
    "download_minidump_stackwalk": False,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],

    # testsuite options
    "suite_definitions": {
        "reftest": {
            "options": [
                "--mulet",
                "--profile=%(gaia_profile)s",
                "--appname=%(application)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--symbols-path=%(symbols_path)s",
                "--enable-oop",
            ],
            "tests": ["%(test_manifest)s"]
        }
    },
    "run_file_names": {
        "reftest": "runreftestb2g.py",
    },
}
