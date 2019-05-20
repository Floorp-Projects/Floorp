# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import os

from manifestparser import TestManifest
from mozlog import get_proxy_logger
from utils import transform_platform

here = os.path.abspath(os.path.dirname(__file__))
raptor_ini = os.path.join(here, 'raptor.ini')
tests_dir = os.path.join(here, 'tests')
LOG = get_proxy_logger(component="raptor-manifest")

LIVE_SITE_TIMEOUT_MULTIPLIER = 1.2

required_settings = [
    'alert_threshold',
    'apps',
    'lower_is_better',
    'measure',
    'page_cycles',
    'test_url',
    'scenario_time',
    'type',
    'unit',
]

playback_settings = [
    'playback_pageset_manifest',
    'playback_recordings',
]

whitelist_live_site_tests = [
    "raptor-youtube-playback",
]


def filter_app(tests, values):
    for test in tests:
        if values["app"] in test['apps']:
            yield test


def filter_live_sites(tests, values):
    # if a test uses live sites only allow it to run if running locally or on try
    # this prevents inadvertently submitting live site data to perfherder
    for test in tests:
        if test.get("use_live_sites", "false") == "true":
            # can run with live sites when running locally
            if values["run_local"] is True:
                yield test
            # can run with live sites if running on try
            elif "hg.mozilla.org/try" in os.environ.get('GECKO_HEAD_REPOSITORY', 'n/a'):
                yield test

            # can run with live sites when white-listed
            elif filter(lambda name: test['name'].startswith(name), whitelist_live_site_tests):
                yield test

            else:
                LOG.warning('%s is not allowed to run with use_live_sites' % test['name'])
        else:
            # not using live-sites so go ahead
            yield test


def get_browser_test_list(browser_app, run_local):
    LOG.info(raptor_ini)
    test_manifest = TestManifest([raptor_ini], strict=False)
    info = {"app": browser_app, "run_local": run_local}
    return test_manifest.active_tests(exists=False,
                                      disabled=False,
                                      filters=[filter_app, filter_live_sites],
                                      **info)


def validate_test_ini(test_details):
    # validate all required test details were found in the test INI
    valid_settings = True

    for setting in required_settings:
        # measure setting not required for benchmark type tests
        if setting == 'measure' and test_details['type'] == 'benchmark':
            continue
        if setting == 'scenario_time' and test_details['type'] != 'scenario':
            continue
        if test_details.get(setting) is None:
            # if page-cycles is not specified, it's ok as long as browser-cycles is there
            if setting == "page-cycles" and test_details.get('browser_cycles') is not None:
                continue
            valid_settings = False
            LOG.error("ERROR: setting '%s' is required but not found in %s"
                      % (setting, test_details['manifest']))

    test_details.setdefault("page_timeout", 30000)

    # if playback is specified, we need more playback settings
    if test_details.get('playback') is not None:
        for setting in playback_settings:
            if test_details.get(setting) is None:
                valid_settings = False
                LOG.error("ERROR: setting '%s' is required but not found in %s"
                          % (setting, test_details['manifest']))

    # if 'alert-on' is specified, we need to make sure that the value given is valid
    # i.e. any 'alert_on' values must be values that exist in the 'measure' ini setting
    if test_details.get('alert_on') is not None:

        # support with or without spaces, i.e. 'measure = fcp, loadtime' or '= fcp,loadtime'
        # convert to a list; and remove any spaces
        test_details['alert_on'] = [_item.strip() for _item in test_details['alert_on'].split(',')]

        # now make sure each alert_on value provided is valid
        for alert_on_value in test_details['alert_on']:
            if alert_on_value not in test_details['measure']:
                LOG.error("ERROR: The 'alert_on' value of '%s' is not valid because "
                          "it doesn't exist in the 'measure' test setting!"
                          % alert_on_value)
                valid_settings = False

    return valid_settings


def write_test_settings_json(args, test_details, oskey):
    # write test settings json file with test details that the control
    # server will provide for the web ext
    test_url = transform_platform(test_details['test_url'], oskey)

    test_settings = {
        "raptor-options": {
            "type": test_details['type'],
            "cold": test_details['cold'],
            "test_url": test_url,
            "expected_browser_cycles": test_details['expected_browser_cycles'],
            "page_cycles": int(test_details['page_cycles']),
            "host": args.host,
        }
    }

    if test_details['type'] == "pageload":
        test_settings['raptor-options']['measure'] = {}

        # test_details['measure'] was already converted to a list in get_raptor_test_list below
        # the 'hero=' line is still a raw string from the test INI
        for m in test_details['measure']:
            test_settings['raptor-options']['measure'][m] = True
            if m == 'hero':
                test_settings['raptor-options']['measure'][m] = [h.strip() for h in
                                                                 test_details['hero'].split(',')]

        if test_details.get("alert_on", None) is not None:
            # alert_on was already converted to list above
            test_settings['raptor-options']['alert_on'] = test_details['alert_on']

    if test_details.get("page_timeout", None) is not None:
        test_settings['raptor-options']['page_timeout'] = int(test_details['page_timeout'])

    test_settings['raptor-options']['unit'] = test_details.get("unit", "ms")

    test_settings['raptor-options']['lower_is_better'] = bool_from_str(
        test_details.get("lower_is_better", "true"))

    # support optional subtest unit/lower_is_better fields
    val = test_details.get('subtest_unit', test_settings['raptor-options']['unit'])
    test_settings['raptor-options']['subtest_unit'] = val
    subtest_lower_is_better = test_details.get('subtest_lower_is_better', None)

    if subtest_lower_is_better is None:
        # default to main test values if not set
        test_settings['raptor-options']['subtest_lower_is_better'] = (
            test_settings['raptor-options']['lower_is_better'])
    else:
        test_settings['raptor-options']['subtest_lower_is_better'] = bool_from_str(
            subtest_lower_is_better)

    if test_details.get("alert_change_type", None) is not None:
        test_settings['raptor-options']['alert_change_type'] = test_details['alert_change_type']

    if test_details.get("alert_threshold", None) is not None:
        test_settings['raptor-options']['alert_threshold'] = float(test_details['alert_threshold'])

    if test_details.get("screen_capture", None) is not None:
        test_settings['raptor-options']['screen_capture'] = test_details.get("screen_capture")

    # if Gecko profiling is enabled, write profiling settings for webext
    if test_details.get("gecko_profile", False):
        threads = ['GeckoMain', 'Compositor']

        # With WebRender enabled profile some extra threads
        if os.getenv('MOZ_WEBRENDER') == '1':
            threads.extend(['Renderer', 'WR'])

        if test_details.get('gecko_profile_threads'):
            test_threads = filter(None, test_details['gecko_profile_threads'].split(','))
            threads.extend(test_threads)

        test_settings['raptor-options'].update({
            'gecko_profile': True,
            'gecko_profile_entries': int(test_details.get('gecko_profile_entries')),
            'gecko_profile_interval': int(test_details.get('gecko_profile_interval')),
            'gecko_profile_threads': ','.join(set(threads)),
        })

    if test_details.get("newtab_per_cycle", None) is not None:
        test_settings['raptor-options']['newtab_per_cycle'] = \
            bool(test_details['newtab_per_cycle'])

    if test_details['type'] == "scenario":
        test_settings['raptor-options']['scenario_time'] = test_details['scenario_time']

    settings_file = os.path.join(tests_dir, test_details['name'] + '.json')
    try:
        with open(settings_file, 'w') as out_file:
            json.dump(test_settings, out_file, indent=4, ensure_ascii=False)
            out_file.close()
    except IOError:
        LOG.info("abort: exception writing test settings json!")


def get_raptor_test_list(args, oskey):
    '''
    A test ini (i.e. raptor-firefox-tp6.ini) will have one or more subtests inside,
    each with it's own name ([the-ini-file-test-section]).

    We want the ability to eiter:
        - run * all * of the subtests listed inside the test ini; - or -
        - just run a single one of those subtests that are inside the ini

    A test name is received on the command line. This will either match the name
    of a single subtest (within an ini) - or - if there's no matching single
    subtest with that name, then the test name provided might be the name of a
    test ini itself (i.e. raptor-firefox-tp6) that contains multiple subtests.

    First look for a single matching subtest name in the list of all availble tests,
    and if it's found we will just run that single subtest.

    Then look at the list of all available tests - each available test has a manifest
    name associated to it - and pull out all subtests whose manifest name matches
    the test name provided on the command line i.e. run all subtests in a specified ini.

    If no tests are found at all then the test name is invalid.
    '''
    tests_to_run = []
    # get list of all available tests for the browser we are testing against
    available_tests = get_browser_test_list(args.app, args.run_local)

    # look for single subtest that matches test name provided on cmd line
    for next_test in available_tests:
        if next_test['name'] == args.test:
            tests_to_run.append(next_test)
            break

    # no matches, so now look for all subtests that come from a test ini
    # manifest that matches the test name provided on the commmand line
    if len(tests_to_run) == 0:
        _ini = args.test + ".ini"
        for next_test in available_tests:
            head, tail = os.path.split(next_test['manifest'])
            if tail == _ini:
                # subtest comes from matching test ini file name, so add it
                tests_to_run.append(next_test)

    # go through each test and set the page-cycles and page-timeout, and some config flags
    # the page-cycles value in the INI can be overriden when debug-mode enabled, when
    # gecko-profiling enabled, or when --page-cycles cmd line arg was used (that overrides all)
    for next_test in tests_to_run:
        LOG.info("configuring settings for test %s" % next_test['name'])
        max_page_cycles = next_test.get('page_cycles', 1)

        if args.gecko_profile is True:
            next_test['gecko_profile'] = True
            LOG.info('gecko-profiling enabled')
            max_page_cycles = 3

            if 'gecko_profile_entries' in args and args.gecko_profile_entries is not None:
                next_test['gecko_profile_entries'] = str(args.gecko_profile_entries)
                LOG.info('gecko-profiling entries set to %s' % args.gecko_profile_entries)

            if 'gecko_profile_interval' in args and args.gecko_profile_interval is not None:
                next_test['gecko_profile_interval'] = str(args.gecko_profile_interval)
                LOG.info('gecko-profiling interval set to %s' % args.gecko_profile_interval)

            if 'gecko_profile_threads' in args and args.gecko_profile_threads is not None:
                threads = filter(None, next_test.get('gecko_profile_threads', '').split(','))
                threads.extend(args.gecko_profile_threads)
                next_test['gecko_profile_threads'] = ','.join(threads)
                LOG.info('gecko-profiling extra threads %s' % args.gecko_profile_threads)

        else:
            # if the gecko profiler is not enabled, ignore all of it's settings
            next_test.pop('gecko_profile_entries', None)
            next_test.pop('gecko_profile_interval', None)
            next_test.pop('gecko_profile_threads', None)

        if args.debug_mode is True:
            next_test['debug_mode'] = True
            LOG.info("debug-mode enabled")
            max_page_cycles = 2

        if args.page_cycles is not None:
            next_test['page_cycles'] = args.page_cycles
            LOG.info("set page-cycles to %d as specified on cmd line" % args.page_cycles)
        else:
            if int(next_test.get('page_cycles', 1)) > max_page_cycles:
                next_test['page_cycles'] = max_page_cycles
                LOG.info("page-cycles set to %d" % next_test['page_cycles'])

        # if --page-timeout was provided on the command line, use that instead of INI
        if args.page_timeout is not None:
            LOG.info("setting page-timeout to %d as specified on cmd line" % args.page_timeout)
            next_test['page_timeout'] = args.page_timeout

        # if --browser-cycles was provided on the command line, use that instead of INI
        if args.browser_cycles is not None:
            LOG.info("setting browser-cycles to %d as specified on cmd line" % args.browser_cycles)
            next_test['browser_cycles'] = args.browser_cycles

        if next_test.get("cold", "false") == "true":
            # when running in cold mode, set browser-cycles to the page-cycles value; as we want
            # the browser to restart between page-cycles; and set page-cycles to 1 as we only
            # want 1 single page-load for every browser-cycle
            next_test['cold'] = True
            next_test['expected_browser_cycles'] = int(next_test['browser_cycles'])
            next_test['page_cycles'] = 1
            # also ensure '-cold' is in test name so perfherder results indicate warm cold-load
            if "-cold" not in next_test['name']:
                next_test['name'] += "-cold"
        else:
            # when running in warm mode, just set test-cycles to 1 and leave page-cycles as/is
            next_test['cold'] = False
            next_test['expected_browser_cycles'] = 1

        # either warm or cold-mode, initialize the starting current 'browser-cycle'
        next_test['browser_cycle'] = 1

        if next_test.get('use_live_sites', "false") == "true":
            # when using live sites we want to turn off playback
            LOG.info("using live sites so turning playback off!")
            next_test['playback'] = None
            LOG.info("using live sites so appending '-live' to the test name")
            next_test['name'] = next_test['name'] + "-live"
            # allow a slightly higher page timeout due to remote page loads
            next_test['page_timeout'] = int(
                next_test['page_timeout']) * LIVE_SITE_TIMEOUT_MULTIPLIER
            LOG.info("using live sites so using page timeout of %dms" % next_test['page_timeout'])

        # convert 'measure =' test INI line to list
        if next_test.get('measure') is not None:
            _measures = []
            for m in [m.strip() for m in next_test['measure'].split(',')]:
                # build the 'measures =' list
                _measures.append(m)
            next_test['measure'] = _measures

            # if using live sites, don't measure hero element as it only exists in recordings
            if 'hero' in next_test['measure'] and \
               next_test.get('use_live_sites', "false") == "true":
                # remove 'hero' from the 'measures =' list
                next_test['measure'].remove('hero')
                # remove the 'hero =' line since no longer measuring hero
                del next_test['hero']

    # write out .json test setting files for the control server to read and send to web ext
    if len(tests_to_run) != 0:
        for test in tests_to_run:
            if validate_test_ini(test):
                write_test_settings_json(args, test, oskey)
            else:
                # test doesn't have valid settings, remove it from available list
                LOG.info("test %s is not valid due to missing settings" % test['name'])
                tests_to_run.remove(test)
    else:
        LOG.critical("abort: specified test name doesn't exist")

    return tests_to_run


def bool_from_str(boolean_string):
    if boolean_string == 'true':
        return True
    elif boolean_string == 'false':
        return False
    else:
        raise ValueError("Expected either 'true' or 'false'")
