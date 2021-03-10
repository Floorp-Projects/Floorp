#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import argparse
import os
import subprocess
import time
from pprint import pprint

DESC = """Measures the duration from process start until the first frame is drawn
using the "TotalTime:" field from `adb shell am start -W`. This script is a python
reimplementation of https://medium.com/androiddevelopers/testing-app-startup-performance-36169c27ee55
with additional functionality.

IMPORTANT: this method does not provide a complete picture of start up. Using
./mach perftest (or the deprecated FNPRMS) is the preferred approach because those
provide more comprehensive views of start up. However, this is useful for lightweight
testing if you know exactly what you're looking for.
"""

DEFAULT_ITER_COUNT = 25

CHANNEL_TO_PKG = {
    'nightly': 'org.mozilla.fenix',
    'beta': 'org.mozilla.firefox.beta',
    'release': 'org.mozilla.firefox',
    'debug': 'org.mozilla.fenix.debug'
}


def parse_args():
    parser = argparse.ArgumentParser(description=DESC, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "release_channel", choices=['nightly', 'beta', 'release', 'debug'], help="the release channel to measure"
    )
    parser.add_argument(
        "startup_type", choices=['cold_main', 'cold_view'], help="the type of start up to measure. see https://wiki.mozilla.org/Performance/Fenix#Terminology for descriptions of cold/warm/hot and main/view"
    )
    parser.add_argument("path", help="the path to save the measurement results; will be overwritten")

    parser.add_argument("-c", "--iter-count", default=DEFAULT_ITER_COUNT, type=int,
                        help="the number of iterations to run. defaults to {}".format(DEFAULT_ITER_COUNT))
    parser.add_argument("-f", "--force", action="store_true", help="overwrite the given path rather than stopping on file existence")
    return parser.parse_args()


def validate_args(args):
    # This helps prevent us from accidentally overwriting previous measurements.
    if not args.force:
        if os.path.exists(args.path):
            raise Exception("Given `path` unexpectedly exists: pick a new path or use --force to overwrite.")


def get_package_id(release_channel):
    package_id = CHANNEL_TO_PKG.get(release_channel)
    if not package_id:
        raise Exception('this should never happen: this should be validated by argparse')
    return package_id


def get_activity_manager_args():
    return ['adb', 'shell', 'am']


def force_stop(pkg_id):
    args = get_activity_manager_args() + ['force-stop', pkg_id]
    subprocess.run(args, check=True)


def disable_startup_profiling():
    # Startup profiling sets the app to the "debug-app" which executes extra code to
    # read a config file off disk that triggers the profiling. Removing the app as a
    # debug app should address that issue but isn't a perfect clean up.
    args = get_activity_manager_args() + ['clear-debug-app']
    subprocess.run(args, check=True)


def get_start_cmd(startup_type, pkg_id):
    args_prefix = get_activity_manager_args() + ['start-activity', '-W', '-n']
    if startup_type == 'cold_main':
        cmd = args_prefix + ['{}/.App'.format(pkg_id)]
    elif startup_type == 'cold_view':
        pkg_activity = '{}/org.mozilla.fenix.IntentReceiverActivity'.format(pkg_id)
        cmd = args_prefix + [
            pkg_activity,
            '-d', 'https://example.com',
            '-a', 'android.intent.action.VIEW'
        ]
    else:
        raise Exception('Should never happen (if argparse is set up correctly')
    return cmd


def measure(pkg_id, start_cmd_args, iter_count):
    # Startup profiling may accidentally be left enabled and throw off the results.
    # To prevent this, we disable it.
    disable_startup_profiling()

    # After an (re)installation, we've observed the app starts up more slowly than subsequent runs.
    # As such, we start it once beforehand to let it settle.
    force_stop(pkg_id)
    subprocess.run(start_cmd_args, check=True, capture_output=True)  # capture_output so it doesn't print to the console.
    time.sleep(5)  # To hopefully reach visual completeness.

    measurements = []
    for i in range(0, iter_count):
        force_stop(pkg_id)
        time.sleep(1)
        proc = subprocess.run(start_cmd_args, check=True, capture_output=True)  # expected to wait for app to start.
        measurements.append(get_measurement_from_stdout(proc.stdout))
    return measurements


def get_measurement_from_stdout(stdout):
    # Sample input:
    #
    # Starting: Intent { cmp=org.mozilla.fenix/.App }
    # Status: ok
    # Activity: org.mozilla.fenix/.App
    # ThisTime: 5662
    # TotalTime: 5662
    # WaitTime: 5680
    # Complete
    total_time_prefix = b'TotalTime: '
    lines = stdout.split(b'\n')
    matching_lines = [line for line in lines if line.startswith(total_time_prefix)]
    if len(matching_lines) != 1:
        raise Exception('Each run should only have one {} but this unexpectedly had more: '.format(total_time_prefix) +
                        matching_lines)
    duration = int(matching_lines[0][len(total_time_prefix):])
    return duration


def save_measurements(path, measurements):
    with open(path, 'w') as f:
        for measurement in measurements:
            f.write(str(measurement) + '\n')


def main():
    args = parse_args()
    validate_args(args)

    # Exceptions and script piping like these are why we prefer mozperftest. :)
    print("Clear the onboarding experience manually, if it's desired and you haven't already done so.")
    print("\nYou can use this script to find the average from the results file: https://github.com/mozilla-mobile/perf-tools/blob/9dd8bf1ea0ea8b2663e21d341a1572c5249c513d/average_times.py")

    pkg_id = get_package_id(args.release_channel)
    start_cmd = get_start_cmd(args.startup_type, pkg_id)
    measurements = measure(pkg_id, start_cmd, args.iter_count)
    save_measurements(args.path, measurements)


if __name__ == '__main__':
    main()
