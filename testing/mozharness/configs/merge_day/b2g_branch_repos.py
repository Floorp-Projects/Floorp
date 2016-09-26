import os
import sys

config = {
    "log_name": "b2g_branch_repos",

    "manifest_repo_url": "git://github.com/mozilla-b2g/b2g-manifest.git",
    "manifest_repo_revision": "master",
    "tools_repo_url": "https://hg.mozilla.org/build/tools",
    "tools_repo_revision": "default",
    "branch_remote_substrings": [
        'github.com/mozilla',
    ],
    "no_branch_repos": [
        "gecko",
        # device conflicts
        "platform_external_qemu",
        "codeaurora_kernel_msm",
        "platform_system_core",
        "platform_hardware_ril",
        "android-sdk",
        "platform_external_wpa_supplicant_8",
        "platform_prebuilts_misc",
        "hardware_qcom_display",
        "device_generic_goldfish",
        "platform_frameworks_av",
        "platform_frameworks_base",
        "android-development",
        "device-hammerhead",
        "device_lge_hammerhead-kernel",
        "platform_bionic",
        "platform_build",
    ],
    "extra_branch_manifest_repos": [
        "gecko",
        "gaia",
    ],
    "branch_order": [
        "master",
    ],
    # outdated/unsupported devices
    "ignored_manifests": [
        "dolphin.xml",
        "otoro.xml",
        "hamachi.xml",
        "helix.xml",
        "keon.xml",
        "peak.xml",
        "galaxy-nexus.xml",
        "galaxy-s2.xml",
        "nexus-s.xml",
        "pandaboard.xml",
        "wasabi.xml",
        "nexus-4.xml",
        "flo.xml",
        "flame.xml",
        "flame-l.xml",
        "nexus-s-4g.xml",
        "rpi.xml",
    ],
    "exes": {
        "hg": [
            "hg", "--config",
            "hostfingerprints.hg.mozilla.org=73:7F:EF:AB:68:0F:49:3F:88:91:F0:B7:06:69:FD:8F:F2:55:C9:56",
        ],
        "gittool.py": [sys.executable, os.getcwd() + "/build/tools/buildfarm/utils/gittool.py"],
    },
}
