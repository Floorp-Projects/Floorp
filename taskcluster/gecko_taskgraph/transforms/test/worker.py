# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from gecko_taskgraph.transforms.base import TransformSequence


# default worker types keyed by instance-size
LINUX_WORKER_TYPES = {
    "large": "t-linux-large",
    "xlarge": "t-linux-xlarge",
    "default": "t-linux-large",
}

# windows worker types keyed by test-platform and virtualization
WINDOWS_WORKER_TYPES = {
    "windows10-32-mingwclang-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-qr": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-shippable-qr": {
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows7-32-devedition-qr": {  # build only, tests have no value
        "virtual": "t-win7-32",
        "virtual-with-gpu": "t-win7-32-gpu",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-32-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-32-shippable-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-aarch64-qr": {
        "virtual": "t-win64-aarch64-laptop",
        "virtual-with-gpu": "t-win64-aarch64-laptop",
        "hardware": "t-win64-aarch64-laptop",
    },
    "windows10-64-devedition": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-shippable": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-shippable-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-devedition-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-asan-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-mingwclang-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-ref-hw-2017": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-ref-hw",
    },
    "windows10-32-2004-mingwclang-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-32-2004-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-32-2004-shippable-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-ccov": {
        "virtual": "win10-64-2004-ssd",
        "virtual-with-gpu": "win10-64-2004-ssd-gpu",
    },
    "windows10-64-2004-ccov-qr": {
        "virtual": "win10-64-2004-ssd",
        "virtual-with-gpu": "win10-64-2004-ssd-gpu",
    },
    "windows10-64-2004-devedition": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-shippable": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-shippable-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-devedition-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-asan-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
    "windows10-64-2004-mingwclang-qr": {
        "virtual": "win10-64-2004",
        "virtual-with-gpu": "win10-64-2004-gpu",
    },
}

# os x worker types keyed by test-platform
MACOSX_WORKER_TYPES = {
    "macosx1014-64": "t-osx-1014",
    "macosx1014-64-power": "t-osx-1014-power",
    "macosx1015-64": "t-osx-1015-r8",
    "macosx1100-64": "t-osx-1100-m1",
}

transforms = TransformSequence()


@transforms.add
def set_worker_type(config, tasks):
    """Set the worker type based on the test platform."""
    for task in tasks:
        # during the taskcluster migration, this is a bit tortured, but it
        # will get simpler eventually!
        test_platform = task["test-platform"]
        if task.get("worker-type") and task.get("worker-type") != "default":
            # This test already has its worker type defined, so just use that (yields below)
            # Unless the value is set to "default", in that case ignore it.
            pass
        elif test_platform.startswith("macosx1014-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1014-64"]
        elif test_platform.startswith("macosx1015-64"):
            if "--power-test" in task["mozharness"]["extra-options"]:
                task["worker-type"] = MACOSX_WORKER_TYPES["macosx1014-64-power"]
            else:
                task["worker-type"] = MACOSX_WORKER_TYPES["macosx1015-64"]
        elif test_platform.startswith("macosx1100-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1100-64"]
        elif test_platform.startswith("win"):
            # figure out what platform the job needs to run on
            if task["virtualization"] == "hardware":
                # some jobs like talos and reftest run on real h/w - those are all win10
                if test_platform.startswith("windows10-64-ref-hw-2017"):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES[
                        "windows10-64-ref-hw-2017"
                    ]
                elif test_platform.startswith("windows10-aarch64-qr"):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES[
                        "windows10-aarch64-qr"
                    ]
                else:
                    win_worker_type_platform = WINDOWS_WORKER_TYPES["windows10-64"]
            else:
                # the other jobs run on a vm which may or may not be a win10 vm
                win_worker_type_platform = WINDOWS_WORKER_TYPES[
                    test_platform.split("/")[0]
                ]
            # now we have the right platform set the worker type accordingly
            task["worker-type"] = win_worker_type_platform[task["virtualization"]]
        elif test_platform.startswith("android-hw-g5"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-g5"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-g5"
        elif test_platform.startswith("android-hw-p2"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-p2"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-p2"
        elif test_platform.startswith("android-hw-a51"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-a51"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-a51"
        elif test_platform.startswith("android-em-7.0-x86"):
            task["worker-type"] = "t-linux-metal"
        elif test_platform.startswith("linux") or test_platform.startswith("android"):
            if task.get("suite", "") in ["talos", "raptor"] and not task[
                "build-platform"
            ].startswith("linux64-ccov"):
                task["worker-type"] = "t-linux-talos-1804"
            else:
                task["worker-type"] = LINUX_WORKER_TYPES[task["instance-size"]]
        else:
            raise Exception(f"unknown test_platform {test_platform}")

        yield task
