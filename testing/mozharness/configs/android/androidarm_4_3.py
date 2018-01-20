# mozharness configuration for Android 4.3 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

import os

config = {
    "deprecated_sdk_path": True,
    "robocop_package_name": "org.mozilla.roboexample.test",
    "marionette_address": "localhost:2828",
    "marionette_test_manifest": "unit-tests.ini",
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidarm_4_3/releng.manifest",
    "emulator_manifest": """
        [
        {
        "size": 38451805,
        "digest": "ac3000aa6846dd1b09a420a1ba84727e393036f49da1419d6c9e5ec2490fc6105c74ca18b616c465dbe693992e2939afd88bc2042d961a9050a3cafd2a673ff4",
        "algorithm": "sha512",
        "filename": "android-sdk_r24.0.2a-linux.tar.gz",
        "unpack": "True"
        }
        ] """,
    "emulator_process_name": "emulator64-arm",
    "emulator_extra_args": "-show-kernel -debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk-linux/platform-tools/adb',
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/../minidumps"
    },
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
    },
}
