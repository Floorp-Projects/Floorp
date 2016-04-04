#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozversion
import os
import sys
import time
import traceback
import urllib
import utils
import mozhttpd

from mozlog import get_proxy_logger

from talos.results import TalosResults
from talos.ttest import TTest
from talos.utils import TalosError, TalosRegression
from talos.config import get_configs, ConfigurationError

# directory of this file
here = os.path.dirname(os.path.realpath(__file__))
LOG = get_proxy_logger()


def useBaseTestDefaults(base, tests):
    for test in tests:
        for item in base:
            if item not in test:
                test[item] = base[item]
                if test[item] is None:
                    test[item] = ''
    return tests


def buildCommandLine(test):
    """build firefox command line options for tp tests"""

    # sanity check pageloader values
    # mandatory options: tpmanifest, tpcycles
    if test['tpcycles'] not in range(1, 1000):
        raise TalosError('pageloader cycles must be int 1 to 1,000')
    if test.get('tpdelay') and test['tpdelay'] not in range(1, 10000):
        raise TalosError('pageloader delay must be int 1 to 10,000')
    if 'tpmanifest' not in test:
        raise TalosError("tpmanifest not found in test: %s" % test)

    # build pageloader command from options
    url = ['-tp', test['tpmanifest']]
    CLI_bool_options = ['tpchrome', 'tpmozafterpaint', 'tpdisable_e10s',
                        'tpnoisy', 'rss', 'tprender', 'tploadnocache',
                        'tpscrolltest']
    CLI_options = ['tpcycles', 'tppagecycles', 'tpdelay', 'tptimeout']
    for key in CLI_bool_options:
        if test.get(key):
            url.append('-%s' % key)
    for key in CLI_options:
        value = test.get(key)
        if value:
            url.extend(['-%s' % key, str(value)])

    # XXX we should actually return the list but since we abuse
    # the url as a command line flag to pass to firefox all over the place
    # will just make a string for now
    return ' '.join(url)


def setup_webserver(webserver):
    """use mozhttpd to setup a webserver"""
    LOG.info("starting webserver on %r" % webserver)

    host, port = webserver.split(':')
    return mozhttpd.MozHttpd(host=host, port=int(port), docroot=here)


def run_tests(config, browser_config):
    """Runs the talos tests on the given configuration and generates a report.
    """
    # get the test data
    tests = config['tests']
    tests = useBaseTestDefaults(config.get('basetest', {}), tests)

    paths = ['profile_path', 'tpmanifest', 'extensions', 'setup', 'cleanup']
    for test in tests:

        # Check for profile_path, tpmanifest and interpolate based on Talos
        # root https://bugzilla.mozilla.org/show_bug.cgi?id=727711
        # Build command line from config
        for path in paths:
            if test.get(path):
                test[path] = utils.interpolate(test[path])
        if test.get('tpmanifest'):
            test['tpmanifest'] = \
                os.path.normpath('file:/%s' % (urllib.quote(test['tpmanifest'],
                                               '/\\t:\\')))
        if not test.get('url'):
            # build 'url' for tptest
            test['url'] = buildCommandLine(test)
        test['url'] = utils.interpolate(test['url'])
        test['setup'] = utils.interpolate(test['setup'])
        test['cleanup'] = utils.interpolate(test['cleanup'])

    # pass --no-remote to firefox launch, if --develop is specified
    # we do that to allow locally the user to have another running firefox
    # instance
    if browser_config['develop']:
        browser_config['extra_args'] = '--no-remote'

    # with addon signing for production talos, we want to develop without it
    if browser_config['develop'] or browser_config['branch_name'] == 'Try':
        browser_config['preferences']['xpinstall.signatures.required'] = False

    # set defaults
    title = config.get('title', '')
    testdate = config.get('testdate', '')

    if browser_config['e10s'] and not title.endswith(".e"):
        # we are running in e10s mode
        title = "%s.e" % (title,)

    # get the process name from the path to the browser
    if not browser_config['process']:
        browser_config['process'] = \
            os.path.basename(browser_config['browser_path'])

    # fix paths to substitute
    # `os.path.dirname(os.path.abspath(__file__))` for ${talos}
    # https://bugzilla.mozilla.org/show_bug.cgi?id=705809
    browser_config['extensions'] = [utils.interpolate(i)
                                    for i in browser_config['extensions']]
    browser_config['bcontroller_config'] = \
        utils.interpolate(browser_config['bcontroller_config'])

    # normalize browser path to work across platforms
    browser_config['browser_path'] = \
        os.path.normpath(browser_config['browser_path'])

    binary = browser_config["browser_path"]
    version_info = mozversion.get_version(binary=binary)
    browser_config['browser_name'] = version_info['application_name']
    browser_config['browser_version'] = version_info['application_version']
    browser_config['buildid'] = version_info['application_buildid']
    try:
        browser_config['repository'] = version_info['application_repository']
        browser_config['sourcestamp'] = version_info['application_changeset']
    except KeyError:
        if not browser_config['develop']:
            print "unable to find changeset or repository: %s" % version_info
            sys.exit()
        else:
            browser_config['repository'] = 'develop'
            browser_config['sourcestamp'] = 'develop'

    # get test date in seconds since epoch
    if testdate:
        date = int(time.mktime(time.strptime(testdate,
                                             '%a, %d %b %Y %H:%M:%S GMT')))
    else:
        date = int(time.time())
    LOG.debug("using testdate: %d" % date)
    LOG.debug("actual date: %d" % int(time.time()))

    # results container
    talos_results = TalosResults()

    # results links
    if not browser_config['develop']:
        results_urls = dict(
            # another hack; datazilla stands for Perfherder
            # and do not require url, but a non empty dict is required...
            datazilla_urls=['local.json'],
        )
    else:
        # local mode, output to files
        results_urls = dict(datazilla_urls=[os.path.abspath('local.json')])
    talos_results.check_output_formats(results_urls)

    httpd = setup_webserver(browser_config['webserver'])
    httpd.start()

    testname = None
    # run the tests
    timer = utils.Timer()
    LOG.suite_start(tests=[test['name'] for test in tests])
    try:
        for test in tests:
            testname = test['name']
            LOG.test_start(testname)

            mytest = TTest()
            talos_results.add(mytest.runTest(browser_config, test))

            LOG.test_end(testname, status='OK')

    except TalosRegression as exc:
        LOG.error("Detected a regression for %s" % testname)
        # by returning 1, we report an orange to buildbot
        # http://docs.buildbot.net/latest/developer/results.html
        LOG.test_end(testname, status='FAIL', message=unicode(exc),
                     stack=traceback.format_exc())
        return 1
    except Exception as exc:
        # NOTE: if we get into this condition, talos has an internal
        # problem and cannot continue
        #       this will prevent future tests from running
        LOG.test_end(testname, status='ERROR', message=unicode(exc),
                     stack=traceback.format_exc())
        # indicate a failure to buildbot, turn the job red
        return 2
    finally:
        LOG.suite_end()
        httpd.stop()

    LOG.info("Completed test suite (%s)" % timer.elapsed())

    # output results
    if results_urls:
        talos_results.output(results_urls)
        if browser_config['develop']:
            print ("Thanks for running Talos locally. Results are in %s"
                   % (results_urls['datazilla_urls']))

    # we will stop running tests on a failed test, or we will return 0 for
    # green
    return 0


def main(args=sys.argv[1:]):
    try:
        config, browser_config = get_configs()
    except ConfigurationError, exc:
        sys.exit("ERROR: %s" % exc)
    sys.exit(run_tests(config, browser_config))


if __name__ == '__main__':
    main()
