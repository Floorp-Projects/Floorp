# mozharness configuration for Android x86 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "emulator_manifest": """
        [
        {
        "size": "237849484",
        "digest": "596db3063758aea49f441ed3dccc5429408e037ba953ab8ea676969c12ed267416a7b1f7d61ad5a06fd2c0fe234131843bd9c57a33b8dd8677aa0b25bbb799d7",
        "algorithm": "sha512",
        "filename": "android-sdk_r26b-linux.tar.gz",
        "unpack": "True"
        }
        ] """,
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-gpu swiftshader -skip-adb-auth -verbose -show-kernel -use-system-libs -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk-linux/platform-tools/adb',
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/emulator:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/../minidumps",
        # "LIBGL_DEBUG": "verbose"
    },
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
    },
}
