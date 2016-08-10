# This is a template config file for b2g emulator unittest production.
# TODO: This could be removed after B2G ICS emulator buildbot builds is turned
#       off, Bug 1209180.
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
        }
    },
    "vcs_output_timeout": 1760,
}
