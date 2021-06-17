#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import argparse
import datetime
import os
import re
import subprocess
import time

DESC = """Measures start up durations using multiple methodologies.

IMPORTANT: each methodology provides a different picture of start up. If you're
not sure which one to use, please ask the perf team!

IMPORTANT: some tests require manual test set up! Read the output carefully.

This system is temporary until mozperftest is ready (e.g. less noisy). As such,
we try to keep this simple and avoid implementing all the things they do.
"""

DEFAULT_ITER_COUNT = 25

CHANNEL_TO_PKG = {
    'nightly': 'org.mozilla.fenix',
    'beta': 'org.mozilla.firefox.beta',
    'release': 'org.mozilla.firefox',
    'debug': 'org.mozilla.fenix.debug'
}

TEST_COLD_MAIN_FF = 'cold_main_first_frame'
TEST_COLD_MAIN_RESTORE = 'cold_main_session_restore'
TEST_COLD_VIEW_FF = 'cold_view_first_frame'
TEST_COLD_VIEW_NAV_START = 'cold_view_nav_start'
TESTS = [TEST_COLD_MAIN_FF, TEST_COLD_MAIN_RESTORE, TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]


def parse_args():
    parser = argparse.ArgumentParser(description=DESC, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        "release_channel", choices=CHANNEL_TO_PKG.keys(), help="the release channel to measure"
    )
    parser.add_argument(
        "test_name", choices=TESTS, help="""the measurement methodology to use. Options:
- {cold_main_ff}: click the app icon & get duration to first frame from 'am start -W'
- {cold_main_restore}: click the app icon & get duration from logcat: START proc to PageStart
- {cold_view_ff}: send a VIEW intent & get duration to first frame from 'am start -W'
- {cold_view_nav_start}: send a VIEW intent & get duration from logcat: START proc to PageStart

Measurements to first frame are a reimplementation of
https://medium.com/androiddevelopers/testing-app-startup-performance-36169c27ee55

See https://wiki.mozilla.org/Performance/Fenix#Terminology for descriptions of cold/warm/hot and main/view""".format(
    cold_main_ff=TEST_COLD_MAIN_FF, cold_main_restore=TEST_COLD_MAIN_RESTORE,
    cold_view_ff=TEST_COLD_VIEW_FF, cold_view_nav_start=TEST_COLD_VIEW_NAV_START,
))
    parser.add_argument("path", help="the path to save the measurement results; will abort if file exists")

    parser.add_argument("-c", "--iter-count", default=DEFAULT_ITER_COUNT, type=int,
                        help="the number of iterations to run. defaults to {}".format(DEFAULT_ITER_COUNT))
    parser.add_argument("-f", "--force", action="store_true", help="overwrite the given path rather than stopping on file existence")

    return parser.parse_args()


def validate_args(args):
    # This helps prevent us from accidentally overwriting previous measurements.
    if not args.force:
        if os.path.exists(args.path):
            raise Exception("Given `path` unexpectedly exists: pick a new path or use --force to overwrite.")


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


def get_start_cmd(test_name, pkg_id):
    args_prefix = get_activity_manager_args() + ['start-activity', '-W', '-n']
    if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_MAIN_RESTORE]:
        cmd = args_prefix + ['{}/.App'.format(pkg_id)]
    elif test_name in [TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]:
        pkg_activity = '{}/org.mozilla.fenix.IntentReceiverActivity'.format(pkg_id)
        cmd = args_prefix + [
            pkg_activity,
            '-d', 'https://example.com',
            '-a', 'android.intent.action.VIEW'
        ]
    else: raise NotImplementedError('method unexpectedly undefined for test_name {}'.format(test_name))
    return cmd


def measure(test_name, pkg_id, start_cmd_args, iter_count):
    # Startup profiling may accidentally be left enabled and throw off the results.
    # To prevent this, we disable it.
    disable_startup_profiling()

    # After an (re)installation, we've observed the app starts up more slowly than subsequent runs.
    # As such, we start it once beforehand to let it settle.
    force_stop(pkg_id)
    subprocess.run(start_cmd_args, check=True, capture_output=True)  # capture_output so it doesn't print to the console.
    time.sleep(5)  # To hopefully reach visual completeness.

    measurements = []
    for _ in range(0, iter_count):
        force_stop(pkg_id)
        time.sleep(1)

        # This is only necessary for nav start tests (to ensure logcat only contains the result from the current run).
        # However, it's not known to be disruptive to other tests (to first frame) so we leave it in.
        subprocess.run(['adb', 'logcat', '-c'], check=True)

        proc = subprocess.run(start_cmd_args, check=True, capture_output=True)  # expected to wait for app to start.
        measurements.append(get_measurement(test_name, pkg_id, proc.stdout))

    return measurements


def get_measurement(test_name, pkg_id, stdout):
    if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_VIEW_FF]:
        measurement = get_measurement_from_am_start_log(stdout)
    elif test_name in [TEST_COLD_VIEW_NAV_START, TEST_COLD_MAIN_RESTORE]:
        time.sleep(4)  # We must sleep until the navigation start event occurs.
        proc = subprocess.run(['adb', 'logcat', '-d'], check=True, capture_output=True)
        measurement = get_measurement_from_nav_start_logcat(pkg_id, proc.stdout)
    else: raise NotImplementedError('method unexpectedly undefined for test_name {}'.format(test_name))
    return measurement


def get_measurement_from_am_start_log(stdout):
    # Sample output:
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


def get_measurement_from_nav_start_logcat(pkg_id, logcat_bytes):
    # Relevant lines:
    # 05-18 14:32:47.366  1759  6003 I ActivityManager: START u0 {act=android.intent.action.VIEW dat=https://example.com/... typ=text/html flg=0x10000000 cmp=org.mozilla.fenix/.IntentReceiverActivity} from uid 2000
    # 05-18 14:32:47.402  1759  6003 I ActivityManager: Start proc 9007:org.mozilla.fenix/u0a170 for activity org.mozilla.fenix/.IntentReceiverActivity
    # 05-18 14:32:50.809  9007  9007 I GeckoSession: handleMessage GeckoView:PageStart uri=
    # 05-18 14:32:50.821  9007  9007 I GeckoSession: handleMessage GeckoView:PageStop uri=null
    def line_to_datetime(line):
        date_str = ' '.join(line.split(' ')[:2])  # e.g. "05-18 14:32:47.366"
        # strptime needs microseconds. logcat outputs millis so we append zeroes
        date_str_with_micros = date_str + '000'
        return datetime.datetime.strptime(date_str_with_micros, '%m-%d %H:%M:%S.%f')

    def get_proc_start_datetime():
        # This regex may not work on older versions of Android: we don't care
        # yet because supporting older versions isn't in our requirements.
        proc_start_re = re.compile('ActivityManager: Start proc \d+:{}/'.format(pkg_id))
        proc_start_lines = [line for line in lines if proc_start_re.search(line)]
        assert len(proc_start_lines) == 1
        return line_to_datetime(proc_start_lines[0])

    def get_page_start_datetime():
        page_start_re = re.compile('GeckoSession: handleMessage GeckoView:PageStart uri=')
        page_start_lines = [line for line in lines if page_start_re.search(line)]
        assert len(page_start_lines) == 2, 'found len=' + str(len(page_start_lines))  # One for about:blank & one for target URL.
        return line_to_datetime(page_start_lines[1])  # 2nd PageStart is for target URL.

    logcat = logcat_bytes.decode('UTF-8')  # Easier to work with and must for strptime.
    lines = logcat.split('\n')

    # We measure the time from process start, rather than the earlier START
    # activity line, because I assume we have no control over the duration
    # before our process starts. If we wanted to put in more time, we could
    # double-check this assumption by seeing what values `am start -W` returns
    # compared to the time stamps.
    elapsed_seconds = (get_page_start_datetime() - get_proc_start_datetime()).total_seconds()  # values < 1s are expressed in decimal.
    elapsed_millis = round(elapsed_seconds * 1000)
    return elapsed_millis


def save_measurements(path, measurements):
    with open(path, 'w') as f:
        for measurement in measurements:
            f.write(str(measurement) + '\n')


def print_preface_text(test_name):
    print("To analyze the results, use this script (we recommend using the median):" +
          "\nhttps://github.com/mozilla-mobile/perf-tools/blob/master/analyze_durations.py")
    if test_name in [TEST_COLD_MAIN_FF]:
        print("\nWARNING: you may wish to clear the onboarding experience manually.")
    elif test_name in [TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]:
        print("\nWARNING: you may wish to reduce the number of open tabs when starting this test")
        print("as this test may leave many additional tabs open which could impact the results.")
    elif test_name in [TEST_COLD_MAIN_RESTORE]:
        print("\nWARNING: ensure at least one tab is opened when starting this test.")


def main():
    args = parse_args()
    validate_args(args)

    pkg_id = CHANNEL_TO_PKG[args.release_channel]
    start_cmd = get_start_cmd(args.test_name, pkg_id)
    print_preface_text(args.test_name)
    measurements = measure(args.test_name, pkg_id, start_cmd, args.iter_count)
    save_measurements(args.path, measurements)


if __name__ == '__main__':
    main()
