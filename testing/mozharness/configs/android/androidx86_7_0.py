# mozharness configuration for Android x86/x86_64 7.0 unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

import os

config = {
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "emulator_manifest": """
        [
        ] """,
    "emulator_avd_name": "test-1",
    "emulator_process_name": "qemu-system-x86_64",
    "emulator_extra_args": "-gpu on -skip-adb-auth -verbose -show-kernel -ranchu -selinux permissive -memory 3072 -cores 4",
    "exes": {
        'adb': '{fetches_dir}/android-sdk-linux/platform-tools/adb'.format(fetches_dir=os.environ['MOZ_FETCHES_DIR']),
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:{fetches_dir}/android-sdk-linux/emulator:{fetches_dir}/android-sdk-linux/tools:{fetches_dir}/android-sdk-linux/platform-tools".format(fetches_dir=os.environ['MOZ_FETCHES_DIR']),
        # "LIBGL_DEBUG": "verbose"
    },
    "bogomips_minimum": 3000,
    # in support of test-verify
    "android_version": 24,
    "is_fennec": False,
    "is_emulator": True,
}
