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
        # device conflict
        "platform_frameworks_base",
        "device_lge_hammerhead-kernel",
        "device-hammerhead",
    ],
    "extra_branch_manifest_repos": [
        "gecko",
        "gaia",
    ],
    "branch_order": [
        "master",
        "b2g-4.3_r2.1",   # prefer jellybean over kitkat for now, since
        "b2g-jellybean",  # most of our builds use jb
        "ics_chocolate_rb4.2",  # prefer wasabi's hardware_qcom_display over
                                # otoro's
        "foxfone-one",  # prefer flame's kernel_lm ove flame-kk's
    ],

    "exes": {
        "hg": [
            "hg", "--config",
            "hostfingerprints.hg.mozilla.org=af:27:b9:34:47:4e:e5:98:01:f6:83:2b:51:c9:aa:d8:df:fb:1a:27",
        ],
        "gittool.py": [sys.executable, os.getcwd() + "/build/tools/buildfarm/utils/gittool.py"],
    },
}
