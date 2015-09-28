import os
import platform

# OS Specifics
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "firefox", "firefox-bin")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.tar.bz2")
XPCSHELL_NAME = "xpcshell"
EXE_SUFFIX = ""
DISABLE_SCREEN_SAVER = True
ADJUST_MOUSE_AND_SCREEN = False
if platform.architecture()[0] == "64bit":
    TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/linux64/releng.manifest"
    MINIDUMP_STACKWALK_PATH = "linux64-minidump_stackwalk"
else:
    TOOLTOOL_MANIFEST_PATH = "config/tooltool-manifests/linux32/releng.manifest"
    MINIDUMP_STACKWALK_PATH = "linux32-minidump_stackwalk"

#####
config = {
    "buildbot_json_path": "buildprops.json",
    "exes": {
        "python": "/tools/buildbot/bin/python",
        "virtualenv": ["/tools/buildbot/bin/python", "/tools/misc-python/virtualenv.py"],
        "tooltool.py": "/tools/tooltool.py",
    },
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    ###
    "installer_path": INSTALLER_PATH,
    "binary_path": BINARY_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "exe_suffix": EXE_SUFFIX,
    "run_file_names": {
        "mochitest": "runtests.py",
        "webapprt": "runtests.py",
        "reftest": "runreftest.py",
        "xpcshell": "runxpcshelltests.py",
        "cppunittest": "runcppunittests.py",
        "gtest": "rungtests.py",
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
        "gtest": ["gtest/*"],
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
        "luciddream-emulator": {
            "options": [
                "--startup-timeout=300",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--browser-path=%(browser_path)s",
                "--b2gpath=%(emulator_path)s",
                "%(test_manifest)s"
            ],
        },
        "luciddream-b2gdt": {
            "options": [
                "--startup-timeout=300",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--browser-path=%(browser_path)s",
                "--b2g-desktop-path=%(fxos_desktop_path)s",
                "--gaia-profile=%(gaia_profile)s",
                "%(test_manifest)s"
            ],
        },
        "mochitest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--setpref=webgl.force-enabled=true",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--use-test-media-devices",
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
                "--testing-modules-dir=test/modules",
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
        },
        "gtest": {
            "options": [
                "--xre-path=%(abs_res_dir)s",
                "--cwd=%(gtest_dir)s",
                "--symbols-path=%(symbols_path)s",
                "%(binary_path)s",
            ],
            "run_filename": "rungtests.py",
        },
    },
    # local mochi suites
    "all_mochitest_suites": {
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
        "reftest": {
            "options": ["--suite=reftest"],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"]
        },
        "crashtest": {
            "options": ["--suite=crashtest"],
            "tests": ["tests/reftest/tests/testing/crashtest/crashtests.list"]
        },
        "jsreftest": {
            "options":["--extra-profile-file=tests/jsreftest/tests/user.js",
                       "--suite=jstestbrowser"],
            "tests": ["tests/jsreftest/tests/jstests.list"]
        },
        "reftest-ipc": {
            "env": {
                "MOZ_OMTC_ENABLED": "1",
                "MOZ_DISABLE_CONTEXT_SHARING_GLX": "1"
            },
            "options": ["--suite=reftest",
                        "--setpref=browser.tabs.remote=true",
                        "--setpref=browser.tabs.remote.autostart=true",
                        "--setpref=layers.offmainthreadcomposition.testing.enabled=true",
                        "--setpref=layers.async-pan-zoom.enabled=true"],
            "tests": ["tests/reftest/tests/layout/reftests/reftest-sanity/reftest.list"]
        },
        "reftest-no-accel": {
            "options": ["--suite=reftest",
                        "--setpref=layers.acceleration.force-enabled=disabled"],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"]},
        "crashtest-ipc": {
            "env": {
                "MOZ_OMTC_ENABLED": "1",
                "MOZ_DISABLE_CONTEXT_SHARING_GLX": "1"
            },
            "options": ["--suite=crashtest",
                        "--setpref=browser.tabs.remote=true",
                        "--setpref=browser.tabs.remote.autostart=true",
                        "--setpref=layers.offmainthreadcomposition.testing.enabled=true",
                        "--setpref=layers.async-pan-zoom.enabled=true"],
            "tests": ["tests/reftest/tests/testing/crashtest/crashtests.list"]
        },
    },
    "all_xpcshell_suites": {
        "xpcshell": {
            "options": ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--manifest=tests/xpcshell/tests/all-test-dirs.list"],
            "tests": []
        },
        "xpcshell-addons": {
            "options": ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--tag=addons",
                        "--manifest=tests/xpcshell/tests/all-test-dirs.list"],
            "tests": []
        },
    },
    "all_cppunittest_suites": {
        "cppunittest": {"tests": ["tests/cppunittest"]}
    },
    "all_gtest_suites": {
        "gtest": []
    },
    "all_jittest_suites": {
        "jittest": [],
        "jittest1": ["--total-chunks=2", "--this-chunk=1"],
        "jittest2": ["--total-chunks=2", "--this-chunk=2"],
        "jittest-chunked": [],
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
            "halt_on_failure": False,
            "architectures": ["32bit", "64bit"],
            "enabled": DISABLE_SCREEN_SAVER
        },
        {
            "name": "run mouse & screen adjustment script",
            "cmd": [
                # when configs are consolidated this python path will only show
                # for windows.
                "python", "../scripts/external_tools/mouse_and_screen_resolution.py",
                "--configuration-url",
                "https://hg.mozilla.org/%(branch)s/raw-file/%(revision)s/" +
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
    "minidump_stackwalk_path": MINIDUMP_STACKWALK_PATH,
    "minidump_tooltool_manifest_path": TOOLTOOL_MANIFEST_PATH,
    "tooltool_cache": "/builds/tooltool_cache",
}
