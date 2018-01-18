# mozharness configuration for Android x86 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

import os

config = {
    "deprecated_sdk_path": True,
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86/releng.manifest",
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
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
        "http_port": "8854",  # starting http port to use for the mochitest server
        "ssl_port": "4454",  # starting ssl port to use for the server
    },
}
