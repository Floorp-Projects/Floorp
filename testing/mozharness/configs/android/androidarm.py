import os

config = {
    "buildbot_json_path": "buildprops.json",
    "host_utils_url": "http://talos-remote.pvt.build.mozilla.org/tegra/tegra-host-utils.Linux.1109310.2.zip",
    "robocop_package_name": "org.mozilla.roboexample.test",
    "device_ip": "127.0.0.1",
    "default_sut_port1": "20701",
    "default_sut_port2": "20700", # does not prompt for commands
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidarm/releng.manifest",
    "tooltool_cache": "/builds/tooltool_cache",
    "emulator_manifest": """
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
    "emulator_extra_args": "-debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket -qemu -m 1024 -cpu cortex-a9",
    "device_manager": "sut",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk18/platform-tools/adb',
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk18/tools:%(abs_work_dir)s/android-sdk18/platform-tools",
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
            "http_port": "8854", # starting http port to use for the mochitest server
            "ssl_port": "4454", # starting ssl port to use for the server
            "emulator_port": 5554,
            "sut_port1": 20701,
            "sut_port2": 20700
        },
    "suite_definitions": {
        "mochitest": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=sut",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
            ],
        },
        "mochitest-gl": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=sut",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
                "--total-chunks=4",
                "--subsuite=webgl",
            ],
        },
        "robocop": {
            "run_filename": "runrobocop.py",
            "testsdir": "mochitest",
            "options": [
                "--dm_trans=sut",
                "--app=%(app)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
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
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path",
                "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=16",
                "--suite=reftest",
            ],
            "tests": ["tests/layout/reftests/reftest.list"],
        },
        "crashtest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path",
                "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=2",
                "--suite=crashtest",
            ],
            "tests": ["tests/testing/crashtest/crashtests.list"],
        },
        "jsreftest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [
                "--app=%(app)s",
                "--ignore-window-size",
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--httpd-path",
                "%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=6",
                "--extra-profile-file=jsreftest/tests/user.js",
                "--suite=jstestbrowser",
            ],
            "tests": ["../jsreftest/tests/jstests.list"],
        },
        "xpcshell": {
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell",
            "options": [
                "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s",
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
    }, # end suite_definitions
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
        "mochitest-1": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=1"],
        },
        "mochitest-2": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=2"],
        },
        "mochitest-3": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=3"],
        },
        "mochitest-4": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=4"],
        },
        "mochitest-5": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=5"],
        },
        "mochitest-6": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=6"],
        },
        "mochitest-7": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=7"],
        },
        "mochitest-8": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=8"],
        },
        "mochitest-9": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=9"],
        },
        "mochitest-10": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=10"],
        },
        "mochitest-11": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=11"],
        },
        "mochitest-12": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=12"],
        },
        "mochitest-13": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=13"],
        },
        "mochitest-14": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=14"],
        },
        "mochitest-15": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=15"],
        },
        "mochitest-16": {
            "category": "mochitest",
            "extra_args": ["--total-chunks=16", "--this-chunk=16"],
        },
        "mochitest-chrome": {
            "category": "mochitest",
            "extra_args": ["--chrome"],
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
        "crashtest-1": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=1"],
        },
        "crashtest-2": {
            "category": "crashtest",
            "extra_args": ["--this-chunk=2"],
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
    }, # end of "test_definitions"
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),
}
