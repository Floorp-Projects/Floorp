# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import re


def init_geckoview_power_test(raptor):
    upload_dir = os.getenv('MOZ_UPLOAD_DIR')
    if not upload_dir:
        raptor.log.critical("Geckoview power test ignored because MOZ_UPLOAD_DIR was not set")
        return
    # Set the screen off timeout to 2 hours since the device will be running
    # disconnected and would otherwise turn off the screen thereby halting
    # execution of the test. Save the current value so we can restore it later
    # since it is a persistent change.
    raptor.screen_off_timeout = raptor.device.shell_output(
        "settings get system screen_off_timeout").strip()
    raptor.device.shell_output("settings put system screen_off_timeout 7200000")
    raptor.device.shell_output("dumpsys batterystats --reset")
    raptor.device.shell_output("dumpsys batterystats --enable full-wake-history")
    filepath = os.path.join(upload_dir, 'battery-before.txt')
    with open(filepath, 'w') as output:
        output.write(raptor.device.shell_output("dumpsys battery"))


def finish_geckoview_power_test(raptor):
    upload_dir = os.getenv('MOZ_UPLOAD_DIR')
    if not upload_dir:
        raptor.log.critical("Geckoview power test ignored because MOZ_UPLOAD_DIR was not set")
        return
    # Restore the screen off timeout.
    raptor.device.shell_output(
        "settings put system screen_off_timeout %s" % raptor.screen_off_timeout)
    filepath = os.path.join(upload_dir, 'battery-after.txt')
    with open(filepath, 'w') as output:
        output.write(raptor.device.shell_output("dumpsys battery"))
    verbose = raptor.device._verbose
    raptor.device._verbose = False
    filepath = os.path.join(upload_dir, 'batterystats.csv')
    with open(filepath, 'w') as output:
        output.write(raptor.device.shell_output("dumpsys batterystats --checkin"))
    filepath = os.path.join(upload_dir, 'batterystats.txt')
    with open(filepath, 'w') as output:
        batterystats = raptor.device.shell_output("dumpsys batterystats")
        output.write(batterystats)
    raptor.device._verbose = verbose
    uid = None
    cpu = wifi = smearing = screen = proportional = 0
    r_uid = re.compile(r'proc=([^:]+):"%s"' % raptor.config['binary'])
    batterystats = batterystats.split('\n')
    for line in batterystats:
        if uid is None:
            match = r_uid.search(line)
            if match:
                uid = match.group(1)
                r_power = re.compile(
                    r'\s+Uid %s:\s+[\d.]+ [(] cpu=([\d.]+) wifi=([\d.]+) [)] '
                    r'Including smearing: ([\d.]+)' % uid)
        else:
            match = r_power.match(line)
            if match:
                (cpu, wifi, smearing) = match.groups()
                r_screen = re.compile(r'screen=([\d.]+)')
                match = r_screen.search(line)
                if match:
                    screen = match.group(1)
                r_proportional = re.compile(r'proportional=([\d.]+)')
                match = r_proportional.search(line)
                if match:
                    proportional = match.group(1)
                break
    raptor.log.info('power data for uid: %s, cpu: %s, wifi: %s, screen: %s, proportional: %s' %
                    (uid, cpu, wifi, screen, proportional))

    # send power data directly to the control server results handler;
    # so it can be formatted and output for perfherder ingestion

    power_data = {'type': 'power',
                  'test': 'raptor-speedometer-geckoview',
                  'unit': 'mAh',
                  'values': {
                      'cpu': float(cpu),
                      'wifi': float(wifi),
                      'screen': float(screen),
                      'proportional': float(proportional)}}

    raptor.log.info("submitting power data via control server directly")
    raptor.control_server.submit_supporting_data(power_data)

    # generate power bugreport zip
    raptor.log.info("generating power bugreport zip")
    raptor.device.command_output(["bugreport", upload_dir])
