# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import re


def init_android_power_test(raptor):
    upload_dir = os.getenv("MOZ_UPLOAD_DIR")
    if not upload_dir:
        raptor.log.critical(
            "% power test ignored; MOZ_UPLOAD_DIR unset" % raptor.config["app"]
        )
        return
    # Set the screen-off timeout to two (2) hours, since the device will be running
    # disconnected, and would otherwise turn off the screen, thereby halting
    # execution of the test. Save the current value so we can restore it later
    # since it is a persistent change.
    raptor.screen_off_timeout = raptor.device.shell_output(
        "settings get system screen_off_timeout"
    ).strip()
    raptor.device.shell_output("settings put system screen_off_timeout 7200000")

    # Set the screen brightness to ~50% for consistency of measurements across
    # devices and save its current value to restore it later. Screen brightness
    # values range from 0 to 255.
    raptor.screen_brightness = raptor.device.shell_output(
        "settings get system screen_brightness"
    ).strip()
    raptor.device.shell_output("settings put system screen_brightness 127")

    raptor.device.shell_output("dumpsys batterystats --reset")
    raptor.device.shell_output("dumpsys batterystats --enable full-wake-history")

    filepath = os.path.join(upload_dir, "battery-before.txt")
    with open(filepath, "w") as output:
        output.write(raptor.device.shell_output("dumpsys battery"))


# The batterystats output for Estimated power use differs
# for Android 7 and Android 8 and later.
#
# Android 7
# Estimated power use (mAh):
#   Capacity: 2100, Computed drain: 625, actual drain: 1197-1218
#   Unaccounted: 572 ( )
#   Uid u0a78: 329 ( cpu=329 )
#   Screen: 190
#   Cell standby: 87.6 ( radio=87.6 )
#   Idle: 4.10
#   Uid 1000: 1.82 ( cpu=0.537 sensor=1.28 )
#   Wifi: 0.800 ( cpu=0.310 wifi=0.490 )
# Android 8
# Estimated power use (mAh):
#   Capacity: 2700, Computed drain: 145, actual drain: 135-162
#   Screen: 68.2 Excluded from smearing
#   Uid u0a208: 61.7 ( cpu=60.5 wifi=1.28 ) Including smearing: 141 ( screen=67.7 proportional=9. )
#   Cell standby: 2.49 ( radio=2.49 ) Excluded from smearing
#   Idle: 1.63 Excluded from smearing
#   Bluetooth: 0.527 ( cpu=0.00319 bt=0.524 ) Including smearing: 0.574 ( proportional=... )
#   Wifi: 0.423 ( cpu=0.343 wifi=0.0800 ) Including smearing: 0.461 ( proportional=0.0375 )
#
# For Android 8, the cpu, wifi, screen, and proportional values are available from
# the Uid line for the app. If the test does not run long enough, it
# appears that the screen value from the Uid will be missing, but the
# standalone Screen value is available.
#
# For Android 7, only the cpu value is available from the Uid line. We
# can use the Screen and Wifi values for Android 7 from the Screen
# and Wifi lines, which might include contributions from the system or
# other apps; however, it should still be useful for spotting changes in power
# usage.
#
# If the energy values from the Uid line for Android 8 are available, they
# will be used. If for any reason either/both screen or wifi power is
# missing, the values from the Screen and Wifi lines will be used.
#
# If only the cpu energy value is available, it will be used
# along with the values from the Screen and Wifi lines.


def finish_android_power_test(raptor, test_name):
    upload_dir = os.getenv("MOZ_UPLOAD_DIR")
    if not upload_dir:
        raptor.log.critical(
            "% power test ignored because MOZ_UPLOAD_DIR was not set" % test_name
        )
        return
    # Restore screen_off_timeout and screen brightness.
    raptor.device.shell_output(
        "settings put system screen_off_timeout %s" % raptor.screen_off_timeout
    )
    raptor.device.shell_output(
        "settings put system screen_brightness %s" % raptor.screen_brightness
    )

    filepath = os.path.join(upload_dir, "battery-after.txt")
    with open(filepath, "w") as output:
        output.write(raptor.device.shell_output("dumpsys battery"))
    verbose = raptor.device._verbose
    raptor.device._verbose = False
    filepath = os.path.join(upload_dir, "batterystats.csv")
    with open(filepath, "w") as output:
        output.write(raptor.device.shell_output("dumpsys batterystats --checkin"))
    filepath = os.path.join(upload_dir, "batterystats.txt")
    with open(filepath, "w") as output:
        batterystats = raptor.device.shell_output("dumpsys batterystats")
        output.write(batterystats)
    raptor.device._verbose = verbose
    estimated_power = False
    uid = None
    total = cpu = wifi = smearing = screen = proportional = 0
    full_screen = 0
    full_wifi = 0
    re_uid = re.compile(r'proc=([^:]+):"%s"' % raptor.config["binary"])
    re_estimated_power = re.compile(r"\s+Estimated power use [(]mAh[)]")
    re_proportional = re.compile(r"proportional=([\d.]+)")
    re_screen = re.compile(r"screen=([\d.]+)")
    re_full_screen = re.compile(r"\s+Screen:\s+([\d.]+)")
    re_full_wifi = re.compile(r"\s+Wifi:\s+([\d.]+)")
    re_power = None
    batterystats = batterystats.split("\n")
    for line in batterystats:
        if uid is None:
            # The proc line containing the Uid and app name appears
            # before the Estimated power line.
            match = re_uid.search(line)
            if match:
                uid = match.group(1)
                re_power = re.compile(
                    r"\s+Uid %s:\s+([\d.]+) ([(] cpu=([\d.]+) wifi=([\d.]+) [)] "
                    r"Including smearing: ([\d.]+))?" % uid
                )
                continue
        if not estimated_power:
            # Do not attempt to parse data until we have seen
            # Estimated Power in the output.
            match = re_estimated_power.match(line)
            if match:
                estimated_power = True
            continue
        if full_screen == 0:
            match = re_full_screen.match(line)
            if match:
                full_screen = match.group(1)
                continue
        if full_wifi == 0:
            match = re_full_wifi.match(line)
            if match:
                full_wifi = match.group(1)
                continue
        if re_power:
            match = re_power.match(line)
            if match:
                (total, android8, cpu, wifi, smearing) = match.groups()
                if android8:
                    # android8 is not None only if the Uid line
                    # contained values for cpu and wifi, which is
                    # true only for Android 8+.
                    match = re_screen.search(line)
                    if match:
                        screen = match.group(1)
                    match = re_proportional.search(line)
                    if match:
                        proportional = match.group(1)
        if full_screen and full_wifi and (cpu and wifi and smearing or total):
            # Stop parsing batterystats once we have a full set of data.
            break

    cpu = total if cpu is None else cpu
    screen = full_screen if screen == 0 else screen
    wifi = full_wifi if wifi is None else wifi

    raptor.log.info(
        "power data for uid: %s, cpu: %s, wifi: %s, screen: %s, proportional: %s"
        % (uid, cpu, wifi, screen, proportional)
    )

    # send power data directly to the control-server results handler,
    # so it can be formatted and out-put for perfherder ingestion

    power_data = {
        "type": "power",
        "test": test_name,
        "unit": "mAh",
        "values": {
            "cpu": float(cpu),
            "wifi": float(wifi),
            "screen": float(screen),
            "proportional": float(proportional),
        },
    }

    raptor.log.info("submitting power data via control server directly")
    raptor.control_server.submit_supporting_data(power_data)

    # generate power bugreport zip
    raptor.log.info("generating power bugreport zip")
    raptor.device.command_output(["bugreport", upload_dir])
