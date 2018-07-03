# mozharness configuration for Android x86 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "tooltool_servers": ['http://taskcluster/relengapi/tooltool/'],
    "emulator_manifest": """
        [
        {
        "size": 135064025,
        "digest": "125678c5b0d93ead8bbf01ba94253e532909417b40637460624cfca34e92f431534fc77a0225e9c4728dcbcf2884a8f7fa1ee059efdfa82d827ca20477d41705",
        "algorithm": "sha512",
        "filename": "android-sdk_r27.1.12-linux-x86emu.tar.gz",
        "unpack": "True"
        }
        ] """,
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
    "emulator": {
        "name": "test-1",
        "device_id": "emulator-5554",
    },
    "marionette_extra": "--emulator",
}
