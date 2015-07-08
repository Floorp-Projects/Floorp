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
    "run_file_names": {
        "mochitest": "runtestsremote.py",
        "reftest": "remotereftest.py",
        "crashtest": "remotereftest.py",
        "jsreftest": "remotereftest.py",
        "robocop": "runtestsremote.py",
        "instrumentation": "runinstrumentation.py",
        "xpcshell": "remotexpcshelltests.py",
        "jittest": "jit_test.py",
        "cppunittest": "remotecppunittests.py"
    },
    "hostutils_url":  "http://talos-remote.pvt.build.mozilla.org/tegra/tegra-host-utils.Linux.1109310.2.zip",
    "verify_path":  "/builds/sut_tools/verify.py",
    "install_app_path":  "/builds/sut_tools/installApp.py",
    "logcat_path":  "/builds/sut_tools/logcat.py",
    # test harness options are located in the gecko tree
    "in_tree_config": "config/mozharness/android_panda_config.py",
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
