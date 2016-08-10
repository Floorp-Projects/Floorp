#!/usr/bin/env python
import os.path
config = {
    # mozharness options
    "application": "b2g",
    "tooltool_cache": "/builds/tooltool_cache",

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

    "run_file_names": {
        "jsreftest": "runreftestb2g.py",
        "mochitest": "runtestsb2g.py",
        "mochitest-chrome": "runtestsb2g.py",
        "reftest": "runreftestb2g.py",
        "crashtest": "runreftestb2g.py",
        "xpcshell": "runtestsb2g.py",
        "cppunittest": "remotecppunittests.py",
        "marionette": "runtests.py"
    },
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--dm_trans=adb",
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(xre_path)s",
                "--addEnv",
                "LD_LIBRARY_PATH=/vendor/lib:/system/lib:/system/b2g",
                "--with-b2g-emulator=%(b2gpath)s",
                 "--emulator=%(emulator)s",
                "."
            ],
            "run_filename": "remotecppunittests.py",
            "testsdir": "cppunittest"
        },
        "crashtest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--suite=crashtest",
            ],
            "tests": ["tests/testing/crashtest/crashtests.list",],
            "run_filename": "runreftestb2g.py",
            "testsdir": "reftest"
        },
        "jsreftest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--extra-profile-file=jsreftest/tests/user.js",
            ],
            "tests": ["jsreftest/tests/jstests.list",],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "mochitest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--certificate-path=%(certificate_path)s",
                "--screenshot-on-fail",
            ],
            "tests": ["%(test_path)s"],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "mochitest-chrome": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--quiet",
                "--flavor=chrome",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--certificate-path=%(certificate_path)s",
                "--screenshot-on-fail",
            ],
            "tests": ["%(test_path)s"],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--enable-oop",
            ],
            "tests": ["tests/layout/reftests/reftest.list",],
            "run_filename": "runreftestsb2g.py",
            "testsdir": "reftest"
        },
        "xpcshell": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--manifest=tests/xpcshell.ini",
                "--use-device-libs",
                "--testing-modules-dir=%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "xpcshell"
        },
        "marionette": {
            "options": [
                "--type=b2g",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--symbols-path=%(symbols_path)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s"
            ],
            "run_filename": "runtests.py",
            "testsdir": "marionette/harness/marionette"
        }
    },
    "vcs_output_timeout": 1760,
}
