# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import os

from manifestparser import TestManifest
from mozlog import get_proxy_logger

here = os.path.abspath(os.path.dirname(__file__))
raptor_ini = os.path.join(here, 'raptor.ini')
tests_dir = os.path.join(here, 'tests')
LOG = get_proxy_logger(component="manifest")


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


def write_test_settings_json(test_details):
    # write test settings json file with test details that the control
    # server will provide for the web ext
    test_settings = {
        "raptor-options": {
            "type": test_details['type'],
            "test_url": test_details['test_url'],
            "page_cycles": int(test_details['page_cycles'])
        }
    }

    if test_details['type'] == "pageload":
        test_settings['raptor-options']['measure'] = {}
        if "fnbpaint" in test_details['measure']:
            test_settings['raptor-options']['measure']['fnbpaint'] = True
        if "fcp" in test_details['measure']:
            test_settings['raptor-options']['measure']['fcp'] = True
        if "hero" in test_details['measure']:
            test_settings['raptor-options']['measure']['hero'] = test_details['hero'].split()
    if test_details.get("page_timeout", None) is not None:
        test_settings['raptor-options']['page_timeout'] = int(test_details['page_timeout'])

    settings_file = os.path.join(tests_dir, test_details['name'] + '.json')
    try:
        with open(settings_file, 'w') as out_file:
            json.dump(test_settings, out_file, indent=4, ensure_ascii=False)
            out_file.close()
    except IOError:
        LOG.info("abort: exception writing test settings json!")


def get_raptor_test_list(args):
    # get a list of available raptor tests, for the browser we're testing on
    available_tests = get_browser_test_list(args.app)
    tests_to_run = []

    # if test name not provided on command line, run all available raptor tests for this browser;
    # if test name provided on command line, make sure it exists, and then only include that one
    if args.test is not None:
        for next_test in available_tests:
            if next_test['name'] == args.test:
                tests_to_run = [next_test]
                break
        if len(tests_to_run) == 0:
            LOG.critical("abort: specified test doesn't exist!")
    else:
        tests_to_run = available_tests

    # write out .json test setting files for the control server to read and send to web ext
    if len(tests_to_run) != 0:
        for test in tests_to_run:
            write_test_settings_json(test)

    return tests_to_run
