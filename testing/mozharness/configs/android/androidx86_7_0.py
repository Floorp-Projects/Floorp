# mozharness configuration for Android x86 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "emulator_manifest": """
        [
        {
        "size": 131698372,
        "digest": "2f62e4f39e2bd858f640b53bbb6cd33de6646f21419d1a9531d9ab5528a7ca6ab6f4cfe370cbb72c4fd475cb9db842a89acdbb9b647d9c0861ee85bc5901dfed",
        "algorithm": "sha512",
        "filename": "android-sdk_r27.3.10-linux-x86emu.tar.gz",
        "unpack": "True"
        }
        ] """,
    "emulator_avd_name": "test-1",
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-gpu swiftshader_indirect -skip-adb-auth -verbose -show-kernel -use-system-libs -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk-linux/platform-tools/adb',
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/emulator:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/../minidumps",
        # "LIBGL_DEBUG": "verbose"
    },
    "marionette_extra": "--emulator",
}
