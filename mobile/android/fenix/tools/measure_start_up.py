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

FOCUS_CHANNEL_TO_PKG = {
    'nightly': 'org.mozilla.focus',  # it seems problematic that this is the same as release.
    'beta': 'org.mozilla.focus.beta',  # only present since post-fenix update.
    'release': 'org.mozilla.focus',
    'debug': 'org.mozilla.focus.debug'
}

FENIX_CHANNEL_TO_PKG = {
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

TEST_URI = 'https://example.com'

PROD_FENIX = 'fenix'
PROD_FOCUS = 'focus'
PRODUCTS = [PROD_FENIX, PROD_FOCUS]


def parse_args():
    parser = argparse.ArgumentParser(description=DESC, formatter_class=argparse.RawTextHelpFormatter)

    assert FENIX_CHANNEL_TO_PKG.keys() == FOCUS_CHANNEL_TO_PKG.keys(), 'should be equal to use one for choices= below'
    parser.add_argument(
        "release_channel", choices=FENIX_CHANNEL_TO_PKG.keys(), help="the release channel to measure"
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

    # We ordinarily wouldn't specify a default because it may cause the user to get results
    # from a product they didn't intend but this script lives in the fenix repo so having fenix
    # as a default would be less confusing for users.
    parser.add_argument("-p", "--product", default=PROD_FENIX, choices=PRODUCTS,
                        help="which product to get measurements from")

    parser.add_argument("-c", "--iter-count", default=DEFAULT_ITER_COUNT, type=int,
                        help="the number of iterations to run. defaults to {}".format(DEFAULT_ITER_COUNT))
    parser.add_argument("-f", "--force", action="store_true",
                        help="overwrite the given path rather than stopping on file existence")

    return parser.parse_args()


def validate_args(args):
    # This helps prevent us from accidentally overwriting previous measurements.
    if not args.force:
        if os.path.exists(args.path):
            raise Exception("Given `path` unexpectedly exists: pick a new path or use --force to overwrite.")


def product_channel_to_pkg_id(product, channel):
    if product == PROD_FENIX:
        pkg_to_channel_map = FENIX_CHANNEL_TO_PKG
    elif product == PROD_FOCUS:
        pkg_to_channel_map = FOCUS_CHANNEL_TO_PKG
    return pkg_to_channel_map[channel]


def get_adb_shell_args():
    return ['adb', 'shell']


def get_activity_manager_args():
    return get_adb_shell_args() + ['am']


def force_stop(pkg_id):
    args = get_activity_manager_args() + ['force-stop', pkg_id]
    subprocess.run(args, check=True)


def disable_startup_profiling():
    # Startup profiling sets the app to the "debug-app" which executes extra code to
    # read a config file off disk that triggers the profiling. Removing the app as a
    # debug app should address that issue but isn't a perfect clean up.
    args = get_activity_manager_args() + ['clear-debug-app']
    subprocess.run(args, check=True)


def get_component_name_for_intent(pkg_id, intent):
    resolve_component_args = (get_adb_shell_args()
                              + ['cmd', 'package', 'resolve-activity', '--brief']
                              + intent + [pkg_id])
    proc = subprocess.run(resolve_component_args, capture_output=True)
    stdout = proc.stdout.splitlines()
    assert len(stdout) == 2, 'expected 2 lines. Got: {}'.format(stdout)
    return stdout[1]


def get_start_cmd(test_name, pkg_id):
    intent_action_prefix = 'android.intent.action.{}'
    if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_MAIN_RESTORE]:
        intent = [
            '-a', intent_action_prefix.format('MAIN'),
            '-c', 'android.intent.category.LAUNCHER',
        ]
    elif test_name in [TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]:
        intent = [
            '-a', intent_action_prefix.format('VIEW'),
            '-d', TEST_URI
        ]

    # You can't launch an app without an pkg_id/activity pair. Instead of
    # hard-coding the activity, which could break on app updates, we ask the
    # system to resolve it for us.
    component_name = get_component_name_for_intent(pkg_id, intent)
    cmd = get_activity_manager_args() + [
        'start-activity',  # this would change to `start` on older API levels like GS5.
        '-W',  # wait for app launch to complete before returning
        '-n', component_name
        ] + intent
    return cmd


def measure(test_name, product, pkg_id, start_cmd_args, iter_count):
    # Startup profiling may accidentally be left enabled and throw off the results.
    # To prevent this, we disable it.
    disable_startup_profiling()

    # After an (re)installation, we've observed the app starts up more slowly than subsequent runs.
    # As such, we start it once beforehand to let it settle.
    force_stop(pkg_id)
    subprocess.run(start_cmd_args, check=True, capture_output=True)  # capture_output so no print to stdout.
    time.sleep(5)  # To hopefully reach visual completeness.

    measurements = []
    for _ in range(0, iter_count):
        force_stop(pkg_id)
        time.sleep(1)

        # This is only necessary for nav start tests (to ensure logcat only contains the result from the current run).
        # However, it's not known to be disruptive to other tests (to first frame) so we leave it in.
        subprocess.run(['adb', 'logcat', '-c'], check=True)

        proc = subprocess.run(start_cmd_args, check=True, capture_output=True)  # expected to wait for app to start.
        measurements.append(get_measurement(test_name, product, pkg_id, proc.stdout))

    return measurements


def get_measurement(test_name, product, pkg_id, stdout):
    if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_VIEW_FF]:
        measurement = get_measurement_from_am_start_log(stdout)
    elif test_name in [TEST_COLD_VIEW_NAV_START, TEST_COLD_MAIN_RESTORE]:
        time.sleep(4)  # We must sleep until the navigation start event occurs.
        proc = subprocess.run(['adb', 'logcat', '-d'], check=True, capture_output=True)
        measurement = get_measurement_from_nav_start_logcat(product, pkg_id, proc.stdout)
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


def get_measurement_from_nav_start_logcat(product, pkg_id, logcat_bytes):
    # Relevant lines:
    # 05-18 14:32:47.366  1759  6003 I ActivityManager: START u0 {act=android.intent.action.VIEW dat=https://example.com/... typ=text/html flg=0x10000000 cmp=org.mozilla.fenix/.IntentReceiverActivity} from uid 2000  # noqa
    # 05-18 14:32:47.402  1759  6003 I ActivityManager: Start proc 9007:org.mozilla.fenix/u0a170 for activity org.mozilla.fenix/.IntentReceiverActivity  # noqa
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
        proc_start_re = re.compile(r'ActivityManager: Start proc \d+:{}/'.format(pkg_id))
        proc_start_lines = [line for line in lines if proc_start_re.search(line)]
        assert len(proc_start_lines) == 1
        return line_to_datetime(proc_start_lines[0])

    def get_page_start_datetime():
        page_start_re = re.compile('GeckoSession: handleMessage GeckoView:PageStart uri=')
        page_start_lines = [line for line in lines if page_start_re.search(line)]
        page_start_line_count = len(page_start_lines)
        page_start_assert_msg = 'found len=' + str(page_start_line_count)

        # In focus versions <= v8.8.2, it logs 3 PageStart lines and these include actual uris.
        # We need to handle our assertion differently due to the different line count.
        #
        # In focus versions >= v8.8.3, this measurement is broken because the logcat were removed.
        is_old_version_of_focus = 'about:blank' in page_start_lines[0] and product == PROD_FOCUS
        if is_old_version_of_focus:
            assert page_start_line_count == 3, page_start_assert_msg  # Lines: about:blank, target URL, target URL.
        else:
            assert page_start_line_count == 2, page_start_assert_msg  # Lines: about:blank, target URL.
        return line_to_datetime(page_start_lines[1])  # 2nd PageStart is for target URL.

    logcat = logcat_bytes.decode('UTF-8')  # Easier to work with and must for strptime.
    lines = logcat.split('\n')

    # We measure the time from process start, rather than the earlier START
    # activity line, because I assume we have no control over the duration
    # before our process starts. If we wanted to put in more time, we could
    # double-check this assumption by seeing what values `am start -W` returns
    # compared to the time stamps.
    #
    # For total_seconds(), values < 1s are expressed in decimal (e.g. .001 is 1ms).
    elapsed_seconds = (get_page_start_datetime() - get_proc_start_datetime()).total_seconds()
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

    pkg_id = product_channel_to_pkg_id(args.product, args.release_channel)
    start_cmd = get_start_cmd(args.test_name, pkg_id)
    print_preface_text(args.test_name)
    measurements = measure(args.test_name, args.product, pkg_id, start_cmd, args.iter_count)
    save_measurements(args.path, measurements)


if __name__ == '__main__':
    main()
