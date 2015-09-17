# This is a template config file for panda android tests on production.
import socket
import os

MINIDUMP_STACKWALK_PATH = "/builds/minidump_stackwalk"

config = {
    # Values for the foopies
    "exes": {
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
    },
    "hostutils_url":  "http://talos-remote.pvt.build.mozilla.org/tegra/tegra-host-utils.Linux.1109310.2.zip",
    "verify_path":  "/builds/sut_tools/verify.py",
    "install_app_path":  "/builds/sut_tools/installApp.py",
    "logcat_path":  "/builds/sut_tools/logcat.py",
    "all_mochitest_suites": {
        "mochitest-1": ["--total-chunks=8", "--this-chunk=1"],
        "mochitest-2": ["--total-chunks=8", "--this-chunk=2"],
        "mochitest-3": ["--total-chunks=8", "--this-chunk=3"],
        "mochitest-4": ["--total-chunks=8", "--this-chunk=4"],
        "mochitest-5": ["--total-chunks=8", "--this-chunk=5"],
        "mochitest-6": ["--total-chunks=8", "--this-chunk=6"],
        "mochitest-7": ["--total-chunks=8", "--this-chunk=7"],
        "mochitest-8": ["--total-chunks=8", "--this-chunk=8"],
        "mochitest-gl": ["--subsuite=webgl"],
    },
    "all_reftest_suites": {
        "reftest-1": ["--total-chunks=8", "--this-chunk=1"],
        "reftest-2": ["--total-chunks=8", "--this-chunk=2"],
        "reftest-3": ["--total-chunks=8", "--this-chunk=3"],
        "reftest-4": ["--total-chunks=8", "--this-chunk=4"],
        "reftest-5": ["--total-chunks=8", "--this-chunk=5"],
        "reftest-6": ["--total-chunks=8", "--this-chunk=6"],
        "reftest-7": ["--total-chunks=8", "--this-chunk=7"],
        "reftest-8": ["--total-chunks=8", "--this-chunk=8"],
    },
    "all_crashtest_suites": {
        "crashtest": []
    },
    "all_jsreftest_suites": {
        "jsreftest-1": ["--total-chunks=3", "--this-chunk=1"],
        "jsreftest-2": ["--total-chunks=3", "--this-chunk=2"],
        "jsreftest-3": ["--total-chunks=3", "--this-chunk=3"],
    },
    "all_robocop_suites": {
        "robocop-1": ["--total-chunks=10", "--this-chunk=1"],
        "robocop-2": ["--total-chunks=10", "--this-chunk=2"],
        "robocop-3": ["--total-chunks=10", "--this-chunk=3"],
        "robocop-4": ["--total-chunks=10", "--this-chunk=4"],
        "robocop-5": ["--total-chunks=10", "--this-chunk=5"],
        "robocop-6": ["--total-chunks=10", "--this-chunk=6"],
        "robocop-7": ["--total-chunks=10", "--this-chunk=7"],
        "robocop-8": ["--total-chunks=10", "--this-chunk=8"],
        "robocop-9": ["--total-chunks=10", "--this-chunk=9"],
        "robocop-10": ["--total-chunks=10", "--this-chunk=10"],
    },
    "all_instrumentation_suites": {
        "browser": ["--suite", "browser"],
        "background": ["--suite", "background"],
    },
    "all_xpcshell_suites": {
        "xpcshell": []
    },
    "all_jittest_suites": {
        "jittest": []
    },
    "all_cppunittest_suites": {
        "cppunittest": ['cppunittest']
    },
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=tests/bin",
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--localBinDir=../tests/bin",
                "--apk=%(apk_path)s",
                "--skip-manifest=../tests/cppunittests/android_cppunittest_manifest.txt"
            ],
            "run_filename": "remotecppunittests.py",
            "testsdir": "cppunittest"
        },
        "crashtest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--suite=crashtest",
            ],
            "tests": ["reftest/tests/testing/crashtest/crashtests.list",],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "jittest": {
            "options": [
                "bin/js",
                "--remote",
                "-j",
                "1",
                "--deviceTransport=sut",
                "--deviceIP=%(device_ip)s",
                "--localLib=../tests/bin",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all"
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test"
        },
        "jsreftest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--extra-profile-file=jsreftest/tests/user.js",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--suite=jstestbrowser",
            ],
            "tests": ["jsreftest/tests/jstests.list",],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "mochitest": {
            "options": [
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--certificate-path=certs",
                "--app=%(app_name)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
            ],
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--suite=reftest",
            ],
            "tests": ["reftest/tests/layout/reftests/reftest.list"],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "robocop": {
            "options": [
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--certificate-path=certs",
                "--app=%(app_name)s",
                "--console-level=INFO",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--robocop-ini=mochitest/robocop.ini"
            ],
            "run_filename": "runrobocop.py",
            "testsdir": "mochitest"
        },
        "xpcshell": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--manifest=xpcshell/tests/xpcshell.ini",
                "--build-info-json=xpcshell/mozinfo.json",
                "--testing-modules-dir=modules",
                "--local-lib-dir=../fennec",
                "--apk=../%(apk_name)s",
                "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
            ],
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell"
        }
    },
    "pip_index": False,
    "buildbot_json_path": "buildprops.json",
    "mobile_imaging_format": "http://mobile-imaging",
    "mozpool_assignee": socket.gethostname(),
    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'download-and-extract',
        'create-virtualenv',
        'request-device',
        'run-test',
        'close-request',
    ],
    "minidump_stackwalk_path": MINIDUMP_STACKWALK_PATH,
    "minidump_save_path": "%(abs_work_dir)s/../minidumps",
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),
}
