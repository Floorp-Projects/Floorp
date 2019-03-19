# mozharness configuration for Android 4.3 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "deprecated_sdk_path": True,
    "robocop_package_name": "org.mozilla.roboexample.test",
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidarm_4_3/releng.manifest",
    "emulator_manifest": """
        [
        {
            "algorithm": "sha512",
            "visibility": "internal",
            "filename": "android-sdk_r24.0.2a-linux.tar.gz",
            "unpack": true,
            "digest": "9b7d4a6fcb33d80884c68e9099a3e11963a79ec0a380a5a9e1a093e630f960d0a5083392c8804121c3ad27ee8ba29ca8df785d19d5a7fdc89458c4e51ada5120",
            "size": 38591399
        }
        ]""",
    "emulator_avd_name": "test-1",
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
    "marionette_extra": "--emulator",
    "bogomips_minimum": 250,
    # in support of test-verify
    "android_version": 18,
    "is_fennec": True,
    "is_emulator": True,
}
