# mozharness configuration for Android x86 7.0 wrench tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "emulator_avd_name": "test-1",
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-gpu on -skip-adb-auth -verbose -show-kernel -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
        'adb': '%(abs_sdk_dir)s/platform-tools/adb',
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_sdk_dir)s/emulator:%(abs_sdk_dir)s/tools:%(abs_sdk_dir)s/platform-tools",
    },
}
