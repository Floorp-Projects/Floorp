# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


def tune_performance(device, log=None, timeout=None):
    """Set various performance-oriented parameters, to reduce jitter.

    This includes some device-specific kernel tweaks.

    For more information, see https://bugzilla.mozilla.org/show_bug.cgi?id=1547135.
    """
    PerformanceTuner(device, log=log, timeout=timeout).tune_performance()


class PerformanceTuner:
    def __init__(self, device, log=None, timeout=None):
        self.device = device
        self.log = log is not None and log or self.device._logger
        self.timeout = timeout

    def tune_performance(self):
        self.log.info("tuning android device performance")
        self.set_svc_power_stayon()
        if self.device.is_rooted:
            device_name = self.device.shell_output(
                "getprop ro.product.model", timeout=self.timeout
            )
            # all commands require root shell from here on
            self.set_scheduler()
            self.set_virtual_memory_parameters()
            self.turn_off_services()
            self.set_cpu_performance_parameters(device_name)
            self.set_gpu_performance_parameters(device_name)
            self.set_kernel_performance_parameters()
        self.device.clear_logcat(timeout=self.timeout)
        self.log.info("android device performance tuning complete")

    def _set_value_and_check_exitcode(self, file_name, value):
        self.log.info("setting {} to {}".format(file_name, value))
        if self.device.shell_bool(
            " ".join(["echo", str(value), ">", str(file_name)]),
            timeout=self.timeout,
        ):
            self.log.info("successfully set {} to {}".format(file_name, value))
        else:
            self.log.warning("command failed")

    def set_svc_power_stayon(self):
        self.log.info("set device to stay awake on usb")
        self.device.shell_bool("svc power stayon usb", timeout=self.timeout)

    def set_scheduler(self):
        self.log.info("setting scheduler to noop")
        scheduler_location = "/sys/block/sda/queue/scheduler"

        self._set_value_and_check_exitcode(scheduler_location, "noop")

    def turn_off_services(self):
        services = [
            "mpdecision",
            "thermal-engine",
            "thermald",
        ]
        for service in services:
            self.log.info(" ".join(["turning off service:", service]))
            self.device.shell_bool(" ".join(["stop", service]), timeout=self.timeout)

        services_list_output = self.device.shell_output(
            "service list", timeout=self.timeout
        )
        for service in services:
            if service not in services_list_output:
                self.log.info(" ".join(["successfully terminated:", service]))
            else:
                self.log.warning(" ".join(["failed to terminate:", service]))

    def set_virtual_memory_parameters(self):
        self.log.info("setting virtual memory parameters")
        commands = {
            "/proc/sys/vm/swappiness": 0,
            "/proc/sys/vm/dirty_ratio": 85,
            "/proc/sys/vm/dirty_background_ratio": 70,
        }

        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value)

    def set_cpu_performance_parameters(self, device_name=None):
        self.log.info("setting cpu performance parameters")
        commands = {}

        if device_name is not None:
            device_name = self.device.shell_output(
                "getprop ro.product.model", timeout=self.timeout
            )

        if device_name == "Pixel 2":
            # MSM8998 (4x 2.35GHz, 4x 1.9GHz)
            # values obtained from:
            #   /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
            #   /sys/devices/system/cpu/cpufreq/policy4/scaling_available_frequencies
            commands.update(
                {
                    "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor": "performance",
                    "/sys/devices/system/cpu/cpufreq/policy4/scaling_governor": "performance",
                    "/sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq": "1900800",
                    "/sys/devices/system/cpu/cpufreq/policy4/scaling_min_freq": "2457600",
                }
            )
        elif device_name == "Moto G (5)":
            # MSM8937(8x 1.4GHz)
            # values obtained from:
            #   /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
            for x in range(0, 8):
                commands.update(
                    {
                        "/sys/devices/system/cpu/cpu{}/"
                        "cpufreq/scaling_governor".format(x): "performance",
                        "/sys/devices/system/cpu/cpu{}/"
                        "cpufreq/scaling_min_freq".format(x): "1401000",
                    }
                )
        else:
            self.log.info(
                "CPU for device with ro.product.model '{}' unknown, not scaling_governor".format(
                    device_name
                )
            )

        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value)

    def set_gpu_performance_parameters(self, device_name=None):
        self.log.info("setting gpu performance parameters")
        commands = {
            "/sys/class/kgsl/kgsl-3d0/bus_split": "0",
            "/sys/class/kgsl/kgsl-3d0/force_bus_on": "1",
            "/sys/class/kgsl/kgsl-3d0/force_rail_on": "1",
            "/sys/class/kgsl/kgsl-3d0/force_clk_on": "1",
            "/sys/class/kgsl/kgsl-3d0/force_no_nap": "1",
            "/sys/class/kgsl/kgsl-3d0/idle_timer": "1000000",
        }

        if not device_name:
            device_name = self.device.shell_output(
                "getprop ro.product.model", timeout=self.timeout
            )

        if device_name == "Pixel 2":
            # Adreno 540 (710MHz)
            # values obtained from:
            #   /sys/devices/soc/5000000.qcom,kgsl-3d0/kgsl/kgsl-3d0/max_clk_mhz
            commands.update(
                {
                    "/sys/devices/soc/5000000.qcom,kgsl-3d0/devfreq/"
                    "5000000.qcom,kgsl-3d0/governor": "performance",
                    "/sys/devices/soc/soc:qcom,kgsl-busmon/devfreq/"
                    "soc:qcom,kgsl-busmon/governor": "performance",
                    "/sys/devices/soc/5000000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_clock_mhz": "710",
                }
            )
        elif device_name == "Moto G (5)":
            # Adreno 505 (450MHz)
            # values obtained from:
            #   /sys/devices/soc/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/max_clock_mhz
            commands.update(
                {
                    "/sys/devices/soc/1c00000.qcom,kgsl-3d0/devfreq/"
                    "1c00000.qcom,kgsl-3d0/governor": "performance",
                    "/sys/devices/soc/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_clock_mhz": "450",
                }
            )
        else:
            self.log.info(
                "GPU for device with ro.product.model '{}' unknown, not setting devfreq".format(
                    device_name
                )
            )

        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value)

    def set_kernel_performance_parameters(self):
        self.log.info("setting kernel performance parameters")
        commands = {
            "/sys/kernel/debug/msm-bus-dbg/shell-client/update_request": "1",
            "/sys/kernel/debug/msm-bus-dbg/shell-client/mas": "1",
            "/sys/kernel/debug/msm-bus-dbg/shell-client/ab": "0",
            "/sys/kernel/debug/msm-bus-dbg/shell-client/slv": "512",
        }
        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value)
