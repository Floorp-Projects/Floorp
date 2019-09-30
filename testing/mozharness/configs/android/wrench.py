# mozharness configuration for Android x86 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "tooltool_servers": ['http://taskcluster/tooltool.mozilla-releng.net/'],
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
    "emulator_process_name": "emulator64-x86",
    "emulator_extra_args": "-gpu on -skip-adb-auth -verbose -show-kernel -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/emulator:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/minidumps",
    },
}
