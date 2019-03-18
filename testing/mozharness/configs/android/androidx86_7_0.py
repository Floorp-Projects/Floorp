# mozharness configuration for Android x86 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "emulator_manifest": """
        [
        {
            "algorithm": "sha512",
            "visibility": "internal",
            "filename": "android-sdk_r27.3.10-linux-x86emu.tar.gz",
            "unpack": true,
            "digest": "1e226ec63fbfd9e33b2522da0f35960773cbfd6ab310167c49a5d5caf8333ac44a692cf2b20e00acceda02a3dca731883d65c4401a0d3eb79fdf11007ec04e8e",
            "size": 131714575
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
