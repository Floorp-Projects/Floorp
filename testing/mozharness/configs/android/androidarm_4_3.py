import os

config = {
    "buildbot_json_path": "buildprops.json",
    "host_utils_url": "http://talos-remote.pvt.build.mozilla.org/tegra/tegra-host-utils.Linux.1109310.2.zip",
    "robocop_package_name": "org.mozilla.roboexample.test",
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidarm_4_3/releng.manifest",
    "tooltool_cache": "/builds/tooltool_cache",
    "avds_dir": "/home/cltbld/.android",
    "emulator_manifest": """
        [
        {
        "size": 140097024,
        "digest": "51781032335c09103e8509b1a558bf22a7119392cf1ea301c49c01bdf21ff0ceb37d260bc1c322cd9b903252429fb01830fc27e4632be30cd345c95bf4b1a39b",
        "algorithm": "sha512",
        "filename": "android-sdk_r24.0.2-linux.tgz",
        "unpack": "True"
        }
        ] """,
    "tools_manifest": """
        [
        {
        "size": 193383673,
        "digest": "6609e8b95db59c6a3ad60fc3dcfc358b2c8ec8b4dda4c2780eb439e1c5dcc5d550f2e47ce56ba14309363070078d09b5287e372f6e95686110ff8a2ef1838221",
        "algorithm": "sha512",
        "filename": "android-sdk18_0.r18moz1.orig.tar.gz",
        "unpack": "True"
        }
        ] """,
    "emulator_process_name": "emulator64-arm",
    "emulator_extra_args": "-show-kernel -debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket",
    "device_manager": "adb",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk18/platform-tools/adb',
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk18/platform-tools",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/../minidumps"
    },
    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'setup-avds',
        'start-emulator',
        'download-and-extract',
        'create-virtualenv',
        'verify-emulator',
        'install',
        'run-tests',
        'stop-emulator',
    ],
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
        "http_port": "8854",  # starting http port to use for the mochitest server
        "ssl_port": "4454",  # starting ssl port to use for the server
        "emulator_port": 5554,
    },
    "suite_definitions": {
        "mochitest": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=adb",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--extra-profile-file=fonts",
                "--screenshot-on-fail",
            ],
        },
        "mochitest-gl": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=adb",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
                "--total-chunks=10",
                "--subsuite=webgl",
            ],
        },
        "mochitest-media": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=adb",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
                "--chunk-by-runtime",
                "--total-chunks=2",
                "--subsuite=media",
            ],
        },
        "robocop": {
            "run_filename": "runrobocop.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=adb",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--total-chunks=4",
                "--robocop-apk=../../robocop.apk",
                "--robocop-ini=robocop.ini",
            ],
        },
        "reftest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path", "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=16",
                "--extra-profile-file=fonts",
                "--suite=reftest",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
            ],
            "tests": ["tests/layout/reftests/reftest.list",],
        },
        "reftest-debug": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path", "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=48",
                "--extra-profile-file=fonts",
                "tests/layout/reftests/reftest.list",
            ],
        },
        "crashtest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path",
                "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=4",
                "--suite=crashtest",
            ],
            "tests": ["tests/testing/crashtest/crashtests.list",],
        },
        "crashtest-debug": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path",
                "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=10",
                "tests/testing/crashtest/crashtests.list",
            ],
        },
        "jsreftest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s", "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s", "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s", "--httpd-path", "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=6",
                "--extra-profile-file=jsreftest/tests/user.js",
                "--suite=jstestbrowser",
            ],
            "tests": ["../jsreftest/tests/jstests.list",],
        },
        "jsreftest-debug": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--dm_trans=adb",
                "--remote-webserver=%(remote_webserver)s", "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s", "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s", "--httpd-path", "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "../jsreftest/tests/jstests.list",
                "--total-chunks=20",
                "--extra-profile-file=jsreftest/tests/user.js",
            ],
        },
        "xpcshell": {
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell",
            "options": [
                "--dm_trans=adb",
                "--xre-path=%(xre_path)s",
                "--testing-modules-dir=%(modules_dir)s",
                "--apk=%(installer_path)s",
                "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--manifest=tests/xpcshell.ini",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--total-chunks=3",
            ],
        },
        "cppunittest": {
            "run_filename": "remotecppunittests.py",
            "testsdir": "cppunittest",
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(xre_path)s",
                "--dm_trans=adb",
                "--localBinDir=../bin",
                "--apk=%(installer_path)s",
                ".",
            ],
        },

    },  # end suite_definitions
    "test_suite_definitions": {
        "jsreftest-1": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=1"],
        },
        "jsreftest-2": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=2"],
        },
        "jsreftest-3": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=3"],
        },
        "jsreftest-4": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=4"],
        },
        "jsreftest-5": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=5"],
        },
        "jsreftest-6": {
            "category": "jsreftest",
            "extra_args": ["--this-chunk=6"],
        },
        "jsreftest-debug-1": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=1"],
        },
        "jsreftest-debug-2": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=2"],
        },
        "jsreftest-debug-3": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=3"],
        },
        "jsreftest-debug-4": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=4"],
        },
        "jsreftest-debug-5": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=5"],
        },
        "jsreftest-debug-6": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=6"],
        },
        "jsreftest-debug-7": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=7"],
        },
        "jsreftest-debug-8": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=8"],
        },
        "jsreftest-debug-9": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=9"],
        },
        "jsreftest-debug-10": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=10"],
        },
        "jsreftest-debug-11": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=11"],
        },
        "jsreftest-debug-12": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=12"],
        },
        "jsreftest-debug-13": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=13"],
        },
        "jsreftest-debug-14": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=14"],
        },
        "jsreftest-debug-15": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=15"],
        },
        "jsreftest-debug-16": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=16"],
        },
        "jsreftest-debug-17": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=17"],
        },
        "jsreftest-debug-18": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=18"],
        },
        "jsreftest-debug-19": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=19"],
        },
        "jsreftest-debug-20": {
            "category": "jsreftest-debug",
            "extra_args": ["--this-chunk=20"],
        },
        "mochitest-1": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=1"],
        },
        "mochitest-2": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=2"],
        },
        "mochitest-3": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=3"],
        },
        "mochitest-4": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=4"],
        },
        "mochitest-5": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=5"],
        },
        "mochitest-6": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=6"],
        },
        "mochitest-7": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=7"],
        },
        "mochitest-8": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=8"],
        },
        "mochitest-9": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=9"],
        },
        "mochitest-10": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=10"],
        },
        "mochitest-11": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=11"],
        },
        "mochitest-12": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=12"],
        },
        "mochitest-13": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=13"],
        },
        "mochitest-14": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=14"],
        },
        "mochitest-15": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=15"],
        },
        "mochitest-16": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=16"],
        },
        "mochitest-17": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=17"],
        },
        "mochitest-18": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=18"],
        },
        "mochitest-19": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=19"],
        },
        "mochitest-20": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=20", "--this-chunk=20"],
        },
        "mochitest-chrome": {
            "category": "mochitest",
            "extra_args": ["--chrome"],
        },
        "mochitest-media-1": {
            "category": "mochitest-media",
            "extra_args": ["--this-chunk=1"],
        },
        "mochitest-media-2": {
            "category": "mochitest-media",
            "extra_args": ["--this-chunk=2"],
        },
        "mochitest-gl-1": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=1"],
        },
        "mochitest-gl-2": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=2"],
        },
        "mochitest-gl-3": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=3"],
        },
        "mochitest-gl-4": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=4"],
        },
        "mochitest-gl-5": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=5"],
        },
        "mochitest-gl-6": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=6"],
        },
        "mochitest-gl-7": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=7"],
        },
        "mochitest-gl-8": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=8"],
        },
        "mochitest-gl-9": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=9"],
        },
        "mochitest-gl-10": {
            "category": "mochitest-gl",
            "extra_args": ["--this-chunk=10"],
        },
        "reftest-1": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=1"],
        },
        "reftest-2": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=2"],
        },
        "reftest-3": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=3"],
        },
        "reftest-4": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=4"],
        },
        "reftest-5": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=5"],
        },
        "reftest-6": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=6"],
        },
        "reftest-7": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=7"],
        },
        "reftest-8": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=8"],
        },
        "reftest-9": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=9"],
        },
        "reftest-10": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=10"],
        },
        "reftest-11": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=11"],
        },
        "reftest-12": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=12"],
        },
        "reftest-13": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=13"],
        },
        "reftest-14": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=14"],
        },
        "reftest-15": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=15"],
        },
        "reftest-16": {
            "category": "reftest",
            "extra_args": ["--total-chunks=16", "--this-chunk=16"],
        },
        "reftest-debug-1": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=1"],
        },
        "reftest-debug-2": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=2"],
        },
        "reftest-debug-3": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=3"],
        },
        "reftest-debug-4": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=4"],
        },
        "reftest-debug-5": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=5"],
        },
        "reftest-debug-6": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=6"],
        },
        "reftest-debug-7": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=7"],
        },
        "reftest-debug-8": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=8"],
        },
        "reftest-debug-9": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=9"],
        },
        "reftest-debug-10": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=10"],
        },
        "reftest-debug-11": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=11"],
        },
        "reftest-debug-12": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=12"],
        },
        "reftest-debug-13": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=13"],
        },
        "reftest-debug-14": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=14"],
        },
        "reftest-debug-15": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=15"],
        },
        "reftest-debug-16": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=16"],
        },
        "reftest-debug-17": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=17"],
        },
        "reftest-debug-18": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=18"],
        },
        "reftest-debug-19": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=19"],
        },
        "reftest-debug-20": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=20"],
        },
        "reftest-debug-21": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=21"],
        },
        "reftest-debug-22": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=22"],
        },
        "reftest-debug-23": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=23"],
        },
        "reftest-debug-24": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=24"],
        },
        "reftest-debug-25": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=25"],
        },
        "reftest-debug-26": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=26"],
        },
        "reftest-debug-27": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=27"],
        },
        "reftest-debug-28": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=28"],
        },
        "reftest-debug-29": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=29"],
        },
        "reftest-debug-30": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=30"],
        },
        "reftest-debug-31": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=31"],
        },
        "reftest-debug-32": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=32"],
        },
        "reftest-debug-33": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=33"],
        },
        "reftest-debug-34": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=34"],
        },
        "reftest-debug-35": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=35"],
        },
        "reftest-debug-36": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=36"],
        },
        "reftest-debug-37": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=37"],
        },
        "reftest-debug-38": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=38"],
        },
        "reftest-debug-39": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=39"],
        },
        "reftest-debug-40": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=40"],
        },
        "reftest-debug-41": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=41"],
        },
        "reftest-debug-42": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=42"],
        },
        "reftest-debug-43": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=43"],
        },
        "reftest-debug-44": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=44"],
        },
        "reftest-debug-45": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=45"],
        },
        "reftest-debug-46": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=46"],
        },
        "reftest-debug-47": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=47"],
        },
        "reftest-debug-48": {
            "category": "reftest-debug",
            "extra_args": ["--total-chunks=48", "--this-chunk=48"],
        },
        "crashtest-1": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=1"],
        },
        "crashtest-2": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=2"],
        },
        "crashtest-3": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=3"],
        },
        "crashtest-4": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=4"],
        },
        "crashtest-debug-1": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=1"],
        },
        "crashtest-debug-2": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=2"],
        },
        "crashtest-debug-3": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=3"],
        },
        "crashtest-debug-4": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=4"],
        },
        "crashtest-debug-5": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=5"],
        },
        "crashtest-debug-6": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=6"],
        },
        "crashtest-debug-7": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=7"],
        },
        "crashtest-debug-8": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=8"],
        },
        "crashtest-debug-9": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=9"],
        },
        "crashtest-debug-10": {
            "category": "crashtest-debug",
            "extra_args": ["--this-chunk=10"],
        },
        "xpcshell-1": {
            "category": "xpcshell",
            "extra_args": ["--total-chunks=3", "--this-chunk=1"],
        },
        "xpcshell-2": {
            "category": "xpcshell",
            "extra_args": ["--total-chunks=3", "--this-chunk=2"],
        },
        "xpcshell-3": {
            "category": "xpcshell",
            "extra_args": ["--total-chunks=3", "--this-chunk=3"],
        },
        "robocop-1": {
            "category": "robocop",
            "extra_args": ["--this-chunk=1"],
        },
        "robocop-2": {
            "category": "robocop",
            "extra_args": ["--this-chunk=2"],
        },
        "robocop-3": {
            "category": "robocop",
            "extra_args": ["--this-chunk=3"],
        },
        "robocop-4": {
            "category": "robocop",
            "extra_args": ["--this-chunk=4"],
        },
        "cppunittest": {
            "category": "cppunittest",
            "extra_args": [],
        },
    },  # end of "test_definitions"
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
}
