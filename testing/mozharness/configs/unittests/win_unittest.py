import os
import sys

# OS Specifics
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "firefox", "firefox.exe")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.zip")
XPCSHELL_NAME = 'xpcshell.exe'
EXE_SUFFIX = '.exe'
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = True
#####
config = {
    "buildbot_json_path": "buildprops.json",
    "exes": {
        'python': sys.executable,
        'virtualenv': [sys.executable, 'c:/mozilla-build/buildbotve/virtualenv.py'],
        'hg': 'c:/mozilla-build/hg/hg',
        'mozinstall': ['%s/build/venv/scripts/python' % os.getcwd(),
                       '%s/build/venv/scripts/mozinstall-script.py' % os.getcwd()],
        'tooltool.py': [sys.executable, 'C:/mozilla-build/tooltool.py'],
    },
    ###
    "installer_path": INSTALLER_PATH,
    "binary_path": BINARY_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "virtualenv_path": 'venv',
    "virtualenv_python_dll": os.path.join(os.path.dirname(sys.executable), "python27.dll"),
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    "exe_suffix": EXE_SUFFIX,
    "run_file_names": {
        "mochitest": "runtests.py",
        "webapprt": "runtests.py",
        "reftest": "runreftest.py",
        "xpcshell": "runxpcshelltests.py",
        "cppunittest": "runcppunittests.py",
        "jittest": "jit_test.py",
        "mozbase": "test.py",
        "mozmill": "runtestlist.py",
    },
    "minimum_tests_zip_dirs": ["bin/*", "certs/*", "modules/*", "mozbase/*", "config/*"],
    "specific_tests_zip_dirs": {
        "mochitest": ["mochitest/*"],
        "webapprt": ["mochitest/*"],
        "reftest": ["reftest/*", "jsreftest/*"],
        "xpcshell": ["xpcshell/*"],
        "cppunittest": ["cppunittest/*"],
        "jittest": ["jit-test/*"],
        "mozbase": ["mozbase/*"],
        "mozmill": ["mozmill/*"],
    },
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(abs_app_dir)s"
            ],
            "run_filename": "runcppunittests.py",
            "testsdir": "cppunittest"
        },
        "jittest": {
            "options": [
                "tests/bin/js",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all"
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test"
        },
        "mochitest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest"
        },
        "mozbase": {
            "options": [
                "-b",
                "%(binary_path)s"
            ],
            "run_filename": "test.py",
            "testsdir": "mozbase"
        },
        "mozmill": {
            "options": [
                "--binary=%(binary_path)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "runtestlist.py",
            "testsdir": "mozmill"
        },
        "reftest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "runreftest.py",
            "testsdir": "reftest"
        },
        "webapprt": {
            "options": [
                "--app=%(app_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--console-level=INFO",
                "--testing-modules-dir=tests/modules",
                "--quiet"
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest"
        },
        "xpcshell": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--test-plugin-path=%(test_plugin_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--utility-path=tests/bin",
            ],
            "run_filename": "runxpcshelltests.py",
            "testsdir": "xpcshell"
        }
    },
    # local mochi suites
    "all_mochitest_suites":
    {
        "plain1": ["--total-chunks=5", "--this-chunk=1", "--chunk-by-dir=4"],
        "plain2": ["--total-chunks=5", "--this-chunk=2", "--chunk-by-dir=4"],
        "plain3": ["--total-chunks=5", "--this-chunk=3", "--chunk-by-dir=4"],
        "plain4": ["--total-chunks=5", "--this-chunk=4", "--chunk-by-dir=4"],
        "plain5": ["--total-chunks=5", "--this-chunk=5", "--chunk-by-dir=4"],
        "plain": [],
        "plain-chunked": ["--chunk-by-dir=4"],
        "mochitest-push": ["--subsuite=push"],
        "chrome": ["--chrome"],
        "browser-chrome": ["--browser-chrome"],
        "browser-chrome-chunked": ["--browser-chrome", "--chunk-by-runtime"],
        "browser-chrome-addons": ["--browser-chrome", "--chunk-by-runtime", "--tag=addons"],
        "mochitest-gl": ["--subsuite=webgl"],
        "mochitest-devtools-chrome": ["--browser-chrome", "--subsuite=devtools"],
        "mochitest-devtools-chrome-chunked": ["--browser-chrome", "--subsuite=devtools", "--chunk-by-runtime"],
        "mochitest-metro-chrome": ["--browser-chrome", "--metro-immersive"],
        "jetpack-package": ["--jetpack-package"],
        "jetpack-addon": ["--jetpack-addon"],
        "a11y": ["--a11y"],
    },
    # local webapprt suites
    "all_webapprt_suites": {
        "chrome": ["--webapprt-chrome", "--browser-arg=-test-mode"],
        "content": ["--webapprt-content"]
    },
    # local reftest suites
    "all_reftest_suites": {
        "reftest": ["tests/reftest/tests/layout/reftests/reftest.list"],
        "crashtest": ["tests/reftest/tests/testing/crashtest/crashtests.list"],
        "jsreftest": ["--extra-profile-file=tests/jsreftest/tests/user.js", "tests/jsreftest/tests/jstests.list"],
        "reftest-ipc": ['--setpref=browser.tabs.remote=true',
                        '--setpref=browser.tabs.remote.autostart=true',
                        '--setpref=layers.async-pan-zoom.enabled=true',
                        'tests/reftest/tests/layout/reftests/reftest-sanity/reftest.list'],
        "reftest-no-accel": ["--setpref=gfx.direct2d.disabled=true", "--setpref=layers.acceleration.disabled=true",
                             "tests/reftest/tests/layout/reftests/reftest.list"],
        "reftest-omtc": ["--setpref=layers.offmainthreadcomposition.enabled=true",
                         "tests/reftest/tests/layout/reftests/reftest.list"],
        "crashtest-ipc": ['--setpref=browser.tabs.remote=true',
                          '--setpref=browser.tabs.remote.autostart=true',
                          '--setpref=layers.async-pan-zoom.enabled=true',
                          'tests/reftest/tests/testing/crashtest/crashtests.list'],
    },
    "all_xpcshell_suites": {
        "xpcshell": ["--manifest=tests/xpcshell/tests/all-test-dirs.list",
                     "%(abs_app_dir)s/" + XPCSHELL_NAME],
        "xpcshell-addons": ["--manifest=tests/xpcshell/tests/all-test-dirs.list",
                            "--tag=addons",
                            "%(abs_app_dir)s/" + XPCSHELL_NAME]
    },
    "all_cppunittest_suites": {
        "cppunittest": ['tests/cppunittest']
    },
    "all_jittest_suites": {
        "jittest": []
    },
    "all_mozbase_suites": {
        "mozbase": []
    },
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        # NOTE 'enabled' is only here while we have unconsolidated configs
        {
            "name": "disable_screen_saver",
            "cmd": ["xset", "s", "off", "s", "reset"],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": False,
            "enabled": DISABLE_SCREEN_SAVER
        },
        {
            "name": "run mouse & screen adjustment script",
            "cmd": [
                # when configs are consolidated this python path will only show
                # for windows.
                sys.executable,
                "../scripts/external_tools/mouse_and_screen_resolution.py",
                "--configuration-url",
                "https://hg.mozilla.org/%(repo_path)s/raw-file/%(revision)s/" +
                    "testing/machine-configuration.json"],
            "architectures": ["32bit"],
            "halt_on_failure": True,
            "enabled": ADJUST_MOUSE_AND_SCREEN
        },
    ],
    "vcs_output_timeout": 1000,
    "minidump_save_path": "%(abs_work_dir)s/../minidumps",
    "buildbot_max_log_size": 52428800,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": "win32-minidump_stackwalk.exe",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/win32/releng.manifest",
}
