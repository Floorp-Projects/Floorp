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

required_settings = ['apps', 'type', 'page_cycles', 'test_url', 'measure',
                     'unit', 'lower_is_better', 'alert_threshold']

playback_settings = ['playback_binary_manifest', 'playback_pageset_manifest',
                     'playback_recordings', 'python3_win_manifest']


def filter_app(tests, values):
    for test in tests:
        if values["app"] in test['apps']:
            yield test


def get_browser_test_list(browser_app):
    LOG.info(raptor_ini)
    test_manifest = TestManifest([raptor_ini], strict=False)
    info = {"app": browser_app}
    return test_manifest.active_tests(exists=False,
                                      disabled=False,
                                      filters=[filter_app],
                                      **info)


def validate_test_ini(test_details):
    # validate all required test details were found in the test INI
    valid_settings = True

    for setting in required_settings:
        # measure setting not required for benchmark type tests
        if setting == 'measure' and test_details['type'] == 'benchmark':
            continue
        if setting not in test_details:
            valid_settings = False
            LOG.error("ERROR: setting '%s' is required but not found in %s"
                      % (setting, test_details['manifest']))

    # if playback is specified, we need more playback settings
    if 'playback' in test_details:
        for setting in playback_settings:
            if setting not in test_details:
                valid_settings = False
                LOG.error("ERROR: setting '%s' is required but not found in %s"
                          % (setting, test_details['manifest']))

    # if 'alert-on' is specified, we need to make sure that the value given is valid
    # i.e. any 'alert_on' values must be values that exist in the 'measure' ini setting
    if 'alert_on' in test_details:

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
            "test_url": test_url,
            "page_cycles": int(test_details['page_cycles']),
            "host": args.host,
        }
    }

    if test_details['type'] == "pageload":
        test_settings['raptor-options']['measure'] = {}

        for m in [m.strip() for m in test_details['measure'].split(',')]:
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

    if test_details.get("lower_is_better", "true") == "false":
        test_settings['raptor-options']['lower_is_better'] = False
    else:
        test_settings['raptor-options']['lower_is_better'] = True

    # support optional subtest unit/lower_is_better fields, default to main test values if not set
    val = test_details.get('subtest_unit', test_settings['raptor-options']['unit'])
    test_settings['raptor-options']['subtest_unit'] = val
    val = test_details.get('subtest_lower_is_better',
                           test_settings['raptor-options']['lower_is_better'])
    if val == "false":
        test_settings['raptor-options']['subtest_lower_is_better'] = False
    else:
        test_settings['raptor-options']['subtest_lower_is_better'] = True

    if test_details.get("alert_threshold", None) is not None:
        test_settings['raptor-options']['alert_threshold'] = float(test_details['alert_threshold'])

    if test_details.get("screen_capture", None) is not None:
        test_settings['raptor-options']['screen_capture'] = test_details.get("screen_capture")

    # if gecko profiling is enabled, write profiling settings for webext
    if test_details.get("gecko_profile", False):
        test_settings['raptor-options']['gecko_profile'] = True
        # when profiling, if webRender is enabled we need to set that, so
        # the runner can add the web render threads to gecko profiling
        test_settings['raptor-options']['gecko_profile_interval'] = \
            float(test_details.get("gecko_profile_interval", 0))
        test_settings['raptor-options']['gecko_profile_entries'] = \
            float(test_details.get("gecko_profile_entries", 0))
        if str(os.getenv('MOZ_WEBRENDER')) == '1':
            test_settings['raptor-options']['webrender_enabled'] = True

    if test_details.get("newtab_per_cycle", None) is not None:
        test_settings['raptor-options']['newtab_per_cycle'] = \
            bool(test_details['newtab_per_cycle'])

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
    available_tests = get_browser_test_list(args.app)

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
        max_page_cycles = next_test['page_cycles']
        if args.gecko_profile is True:
            next_test['gecko_profile'] = True
            LOG.info("gecko-profiling enabled")
            max_page_cycles = 3
        if args.debug_mode is True:
            next_test['debug_mode'] = True
            LOG.info("debug-mode enabled")
            max_page_cycles = 2
        if args.page_cycles is not None:
            next_test['page_cycles'] = args.page_cycles
            LOG.info("set page-cycles to %d as specified on cmd line" % args.page_cycles)
        else:
            if int(next_test['page_cycles']) > max_page_cycles:
                next_test['page_cycles'] = max_page_cycles
                LOG.info("page-cycles set to %d" % next_test['page_cycles'])
        # if --page-timeout was provided on the command line, use that instead of INI
        if args.page_timeout is not None:
            LOG.info("setting page-timeout to %d as specified on cmd line" % args.page_timeout)
            next_test['page_timeout'] = args.page_timeout

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
