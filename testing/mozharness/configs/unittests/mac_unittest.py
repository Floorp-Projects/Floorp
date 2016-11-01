import os

# OS Specifics
INSTALLER_PATH = os.path.join(os.getcwd(), "installer.dmg")
XPCSHELL_NAME = 'xpcshell'
EXE_SUFFIX = ''
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = False
#####
config = {
    "buildbot_json_path": "buildprops.json",
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
    ###
    "installer_path": INSTALLER_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "exe_suffix": EXE_SUFFIX,
    "run_file_names": {
        "mochitest": "runtests.py",
        "reftest": "runreftest.py",
        "xpcshell": "runxpcshelltests.py",
        "cppunittest": "runcppunittests.py",
        "gtest": "rungtests.py",
        "jittest": "jit_test.py",
        "mozbase": "test.py",
        "mozmill": "runtestlist.py",
    },
    "minimum_tests_zip_dirs": [
        "bin/*",
        "certs/*",
        "config/*",
        "mach",
        "marionette/*",
        "modules/*",
        "mozbase/*",
        "tools/*",
    ],
    "specific_tests_zip_dirs": {
        "mochitest": ["mochitest/*"],
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
                "--xre-path=%(abs_res_dir)s"
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
                "--jitflags=all",
                "--timeout=970" # Keep in sync with run_timeout below.
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test",
            "run_timeout": 1000 # Keep in sync with --timeout above.
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
                "--cleanup-crashes",
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
                "--plugins-path=%(test_plugin_path)s",
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
                "--symbols-path=%(symbols_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--cleanup-crashes",
            ],
            "run_filename": "runreftest.py",
            "testsdir": "reftest"
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
                "--utility-path=tests/bin",
                "%(binary_path)s",
            ],
            "run_filename": "rungtests.py",
        },
    },
    # local mochi suites
    "all_mochitest_suites": {
        "plain": [],
        "plain-gpu": ["--subsuite=gpu"],
        "plain-clipboard": ["--subsuite=clipboard"],
        "plain-chunked": ["--chunk-by-dir=4"],
        "mochitest-media": ["--subsuite=media"],
        "chrome": ["--flavor=chrome"],
        "chrome-gpu": ["--flavor=chrome", "--subsuite=gpu"],
        "chrome-clipboard": ["--flavor=chrome", "--subsuite=clipboard"],
        "chrome-chunked": ["--flavor=chrome", "--chunk-by-dir=4"],
        "browser-chrome": ["--flavor=browser"],
        "browser-chrome-gpu": ["--flavor=browser", "--subsuite=gpu"],
        "browser-chrome-clipboard": ["--flavor=browser", "--subsuite=clipboard"],
        "browser-chrome-chunked": ["--flavor=browser", "--chunk-by-runtime"],
        "browser-chrome-addons": ["--flavor=browser", "--chunk-by-runtime", "--tag=addons"],
        "browser-chrome-screenshots": ["--flavor=browser", "--subsuite=screenshots"],
        "mochitest-gl": ["--subsuite=webgl"],
        "mochitest-devtools-chrome": ["--flavor=browser", "--subsuite=devtools"],
        "mochitest-devtools-chrome-chunked": ["--flavor=browser", "--subsuite=devtools", "--chunk-by-runtime"],
        "jetpack-package": ["--flavor=jetpack-package"],
        "jetpack-package-clipboard": ["--flavor=jetpack-package", "--subsuite=clipboard"],
        "jetpack-addon": ["--flavor=jetpack-addon"],
        "a11y": ["--flavor=a11y"],
    },
    # local reftest suites
    "all_reftest_suites": {
        "reftest": {
            'options': ["--suite=reftest"],
            'tests': ["tests/reftest/tests/layout/reftests/reftest.list"]
        },
        "crashtest": {
            'options': ["--suite=crashtest"],
            'tests': ["tests/reftest/tests/testing/crashtest/crashtests.list"]
        },
        "jsreftest": {
            'options':["--extra-profile-file=tests/jsreftest/tests/user.js"],
            'tests': ["tests/jsreftest/tests/jstests.list"]
        },
        "reftest-ipc": {
            'options': ['--suite=reftest',
                        '--setpref=browser.tabs.remote=true',
                        '--setpref=browser.tabs.remote.autostart=true',
                        '--setpref=extensions.e10sBlocksEnabling=false',
                        '--setpref=layers.async-pan-zoom.enabled=true'],
            'tests': ['tests/reftest/tests/layout/reftests/reftest-sanity/reftest.list']
        },
        "crashtest-ipc": {
            'options': ['--suite=crashtest',
                        '--setpref=browser.tabs.remote=true',
                        '--setpref=browser.tabs.remote.autostart=true',
                        '--setpref=extensions.e10sBlocksEnabling=false',
                        '--setpref=layers.async-pan-zoom.enabled=true'],
            'tests': ['tests/reftest/tests/testing/crashtest/crashtests.list']
        },
    },
    "all_xpcshell_suites": {
        "xpcshell": {
            'options': ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--manifest=tests/xpcshell/tests/xpcshell.ini"],
            'tests': []
        },
        "xpcshell-addons": {
            'options': ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--tag=addons",
                        "--manifest=tests/xpcshell/tests/xpcshell.ini"],
            'tests': []
        },
    },
    "all_cppunittest_suites": {
        "cppunittest": ['tests/cppunittest']
    },
    "all_gtest_suites": {
        "gtest": []
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
                "python", "../scripts/external_tools/mouse_and_screen_resolution.py",
                "--configuration-file",
                "../scripts/external_tools/machine-configuration.json"],
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
    "unstructured_flavors": {"mochitest": ['jetpack'],
                            "xpcshell": [],
                            "gtest": [],
                            "mozmill": [],
                            "cppunittest": [],
                            "jittest": [],
                            "mozbase": [],
                            },
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": "macosx64-minidump_stackwalk",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/macosx64/releng.manifest",
    "tooltool_cache": "/builds/tooltool_cache",
}
