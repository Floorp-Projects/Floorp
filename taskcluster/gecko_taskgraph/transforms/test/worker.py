# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

# default worker types keyed by instance-size
LINUX_WORKER_TYPES = {
    "large": "t-linux-large",
    "large-noscratch": "t-linux-large-noscratch",
    "xlarge": "t-linux-xlarge",
    "xlarge-noscratch": "t-linux-xlarge-noscratch",
    "default": "t-linux-large-noscratch",
}

# windows worker types keyed by test-platform and virtualization
WINDOWS_WORKER_TYPES = {
    "windows10-64": {  # source-test
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows10-64-shippable-qr": {
        "virtual": "t-win10-64",
        "virtual-with-gpu": "t-win10-64-gpu-s",
        "hardware": "t-win10-64-1803-hw",
    },
    "windows11-64-2009-hw-ref-shippable": {
        "virtual": "win11-64-2009-hw-ref",
        "virtual-with-gpu": "win11-64-2009-hw-ref",
        "hardware": "win11-64-2009-hw-ref",
    },
    "windows11-64-2009-hw-ref": {
        "virtual": "win11-64-2009-hw-ref",
        "virtual-with-gpu": "win11-64-2009-hw-ref",
        "hardware": "win11-64-2009-hw-ref",
    },
    "windows10-64-2009-qr": {
        "virtual": "win10-64-2009",
        "virtual-with-gpu": "win10-64-2009-gpu",
    },
    "windows10-64-2009-shippable-qr": {
        "virtual": "win10-64-2009",
        "virtual-with-gpu": "win10-64-2009-gpu",
    },
    "windows11-32-2009-mingwclang-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-32-2009-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-32-2009-shippable-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-ccov": {
        "virtual": "win11-64-2009-ssd",
        "virtual-with-gpu": "win11-64-2009-ssd-gpu",
    },
    "windows11-64-2009-ccov-qr": {
        "virtual": "win11-64-2009-ssd",
        "virtual-with-gpu": "win11-64-2009-ssd-gpu",
    },
    "windows11-64-2009-devedition": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-shippable": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-shippable-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-devedition-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-asan-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
    "windows11-64-2009-mingwclang-qr": {
        "virtual": "win11-64-2009",
        "virtual-with-gpu": "win11-64-2009-gpu",
    },
}

# os x worker types keyed by test-platform
MACOSX_WORKER_TYPES = {
    "macosx1015-64": "t-osx-1015-r8",
    "macosx1100-64": "t-osx-1100-m1",
    "macosx1400-64": "t-osx-1400-m2",
}

transforms = TransformSequence()


@transforms.add
def set_worker_type(config, tasks):
    """Set the worker type based on the test platform."""
    for task in tasks:
        # during the taskcluster migration, this is a bit tortured, but it
        # will get simpler eventually!
        test_platform = task["test-platform"]
        if task.get("worker-type", "default") != "default":
            # This test already has its worker type defined, so just use that (yields below)
            # Unless the value is set to "default", in that case ignore it.
            pass
        elif test_platform.startswith("macosx1015-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1015-64"]
        elif test_platform.startswith("macosx1100-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1100-64"]
        elif test_platform.startswith("macosx1400-64"):
            task["worker-type"] = MACOSX_WORKER_TYPES["macosx1400-64"]
        elif test_platform.startswith("win"):
            # figure out what platform the job needs to run on
            if task["virtualization"] == "hardware":
                # some jobs like talos and reftest run on real h/w
                if test_platform.startswith("windows11-64-2009-hw-ref"):
                    win_worker_type_platform = WINDOWS_WORKER_TYPES[
                        "windows11-64-2009-hw-ref"
                    ]
                else:
                    win_worker_type_platform = WINDOWS_WORKER_TYPES["windows10-64"]
            else:
                # the other jobs run on a vm which may or may not be a win10 vm
                win_worker_type_platform = WINDOWS_WORKER_TYPES[
                    test_platform.split("/")[0]
                ]
                if task[
                    "virtualization"
                ] == "virtual-with-gpu" and test_platform.startswith("windows1"):
                    # some unittests can run on hardware, no need for --requires-gpu
                    if not test_platform.startswith("windows11-64-2009-hw-ref"):
                        # add in `--requires-gpu` to the mozharness options
                        task["mozharness"]["extra-options"].append("--requires-gpu")

            # now we have the right platform set the worker type accordingly
            task["worker-type"] = win_worker_type_platform[task["virtualization"]]
        elif test_platform.startswith("android-hw-p5"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-p5"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-p5"
        elif test_platform.startswith("android-hw-p6"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-p6"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-p6"
        elif test_platform.startswith("android-hw-s21"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-s21"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-s21"
        elif test_platform.startswith("android-hw-a51"):
            if task["suite"] != "raptor":
                task["worker-type"] = "t-bitbar-gw-unit-a51"
            else:
                task["worker-type"] = "t-bitbar-gw-perf-a51"
        elif test_platform.startswith("android-em-7.0-x86"):
            task["worker-type"] = "t-linux-kvm"
        elif test_platform.startswith("linux") or test_platform.startswith("android"):
            if "wayland" in test_platform:
                if task["instance-size"].startswith("xlarge"):
                    task["worker-type"] = "t-linux-xlarge-wayland"
                else:
                    task["worker-type"] = "t-linux-wayland"
            elif task.get("suite", "") in ["talos", "raptor"] and not task[
                "build-platform"
            ].startswith("linux64-ccov"):
                task["worker-type"] = "t-linux-talos-1804"
            else:
                task["worker-type"] = LINUX_WORKER_TYPES[task["instance-size"]]
        else:
            raise Exception(f"unknown test_platform {test_platform}")

        yield task


@transforms.add
def set_wayland_env(config, tasks):
    for task in tasks:
        if "wayland" not in task["test-platform"]:
            yield task
            continue

        env = task.setdefault("worker", {}).setdefault("env", {})
        env["MOZ_ENABLE_WAYLAND"] = "1"
        env["WAYLAND_DISPLAY"] = "wayland-0"
        yield task
