import os

config = {
    "buildbot_json_path": "buildprops.json",
    "hostutils_manifest_path": "testing/config/tooltool-manifests/linux64/hostutils.manifest",
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86/releng.manifest",
    "tooltool_cache": "/home/worker/tooltool_cache",
    "download_tooltool": True,
    "tooltool_servers": ['http://relengapi/tooltool/'],
    "avds_dir": "/home/worker/workspace/build/.android",
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
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-show-kernel -debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket -qemu -m 1024",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk18/platform-tools/adb',
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
    ],
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
        "http_port": "8854",  # starting http port to use for the mochitest server
        "ssl_port": "4454",  # starting ssl port to use for the server
        "emulator_port": 5554,
    },
    "suite_definitions": {
        "xpcshell": {
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell",
            "install": False,
            "options": [
                "--xre-path=%(xre_path)s",
                "--testing-modules-dir=%(modules_dir)s",
                "--apk=%(installer_path)s",
                "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--manifest=tests/xpcshell.ini",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--test-plugin-path=none",
            ],
        },
        "mochitest-chrome": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": [
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
                "--extra-profile-file=hyphenation",
                "--screenshot-on-fail",
                "--flavor=chrome",
            ],
        },
    },  # end suite_definitions
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
}
