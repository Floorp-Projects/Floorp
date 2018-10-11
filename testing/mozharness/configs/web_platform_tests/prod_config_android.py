# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import os

config = {
    "options": [
        "--prefs-root=%(test_path)s/prefs",
        "--processes=1",
        "--config=%(test_path)s/wptrunner.ini",
        "--ca-cert-path=%(test_path)s/tests/tools/certs/cacert.pem",
        "--host-key-path=%(test_path)s/tests/tools/certs/web-platform.test.key",
        "--host-cert-path=%(test_path)s/tests/tools/certs/web-platform.test.pem",
        "--certutil-binary=%(xre_path)s/certutil",
        "--product=fennec",
    ],
    "avds_dir": "/builds/worker/workspace/build/.android",
    "binary_path": "/tmp",
    "download_minidump_stackwalk": False,
    "emulator_avd_name": "test-1",
    "emulator_extra_args": "-gpu swiftshader_indirect -skip-adb-auth -verbose -show-kernel -use-system-libs -ranchu -selinux permissive -memory 3072 -cores 4",
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
    "env": {
        "PATH": "%(PATH)s:%(abs_work_dir)s/android-sdk-linux/emulator:%(abs_work_dir)s/android-sdk-linux/tools:%(abs_work_dir)s/android-sdk-linux/platform-tools",
    },
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk-linux/platform-tools/adb',
    },
    "geckodriver": "%(abs_test_bin_dir)s/geckodriver",
    "hostutils_manifest_path": "testing/config/tooltool-manifests/linux64/hostutils.manifest",
    "log_tbpl_level": "info",
    "log_raw_level": "info",
    "minidump_stackwalk_path": "/usr/local/bin/linux64-minidump_stackwalk",
    "per_test_category": "web-platform",
    "tooltool_cache": os.environ.get("TOOLTOOL_CACHE"),
    "tooltool_manifest_path": "testing/config/tooltool-manifests/androidx86_7_0/releng.manifest",
    "tooltool_servers": ['http://relengapi/tooltool/'],
}
