# mozharness configuration for Android x86/x86_64 7.0 unit tests
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
            "filename": "android-sdk_r29.2.1.0-linux-x86emu.tar.gz",
            "unpack": true,
            "digest": "4014389d2e0c6889edf89a714e4defbd42c2bced79eee1cce726a9b2c921c6d857723f918a9f1b7dca35b9f8d6cbfdf6b47d2934d800bdd396bf5c17ada3b827",
            "size": 299610245
        }
        ] """,
    "emulator_avd_name": "test-1",
    "emulator_process_name": "qemu-system-x86_64",
    "emulator_extra_args": "-gpu on -skip-adb-auth -verbose -show-kernel -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk-linux/platform-tools/adb',
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/emulator:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
        # "LIBGL_DEBUG": "verbose"
    },
    "bogomips_minimum": 3000,
    # in support of test-verify
    "android_version": 24,
    "is_fennec": False,
    "is_emulator": True,
}
