# This is a template config file for b2g desktop unittest production.
import os

config = {
    # mozharness options
    "application": "b2g",
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

    # testsuite options
    "run_file_names": {
        "mochitest": "runtestsb2g.py",
        "reftest": "runreftestb2g.py",
    },
    "suite_definitions": {
        "mochitest": {
            "options": [
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--profile=%(gaia_profile)s",
                "--app=%(application)s",
                "--desktop",
                "--utility-path=%(utility_path)s",
                "--certificate-path=%(cert_path)s",
                "--symbols-path=%(symbols_path)s",
                "--browser-arg=%(browser_arg)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--desktop",
                "--profile=%(gaia_profile)s",
                "--appname=%(application)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--browser-arg=%(browser_arg)s",
                "--symbols-path=%(symbols_path)s",
                "%(test_manifest)s"
            ],
            "run_filename": "runreftestsb2g.py",
            "testsdir": "reftest"
        }
    },
}
