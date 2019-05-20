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
            "filename": "android-sdk_r28.0.25.0-linux-x86emu.tar.gz",
            "unpack": true,
            "digest": "e62acc91f41ccef65a4937a2672fcb56362e9946b806bacc25854035b57d5bd2d525a9c7d660a643ab6381ae2e3b660be7fea70e302ed314c4b07880b2328e18",
            "size": 241459387
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
    "bogomips_minimum": 4000,
    # in support of test-verify
    "android_version": 24,
    "is_fennec": False,
    "is_emulator": True,
}
