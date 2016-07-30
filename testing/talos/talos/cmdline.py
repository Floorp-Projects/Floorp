# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os

from mozlog.commandline import add_logging_group


class _StopAction(argparse.Action):
    def __init__(self, option_strings, dest=argparse.SUPPRESS,
                 default=argparse.SUPPRESS, help=None):
        super(_StopAction, self).__init__(
            option_strings=option_strings,
            dest=dest,
            default=default,
            nargs=0,
            help=help)


class _ListTests(_StopAction):
    def __call__(self, parser, namespace, values, option_string=None):
        from talos import test
        print('Available tests:')
        print('================\n')
        test_class_names = [
            (test_class.name(), test_class.description())
            for test_class in test.test_dict().itervalues()
        ]
        test_class_names.sort()
        for name, description in test_class_names:
            print(name)
            print('-'*len(name))
            print(description)
            print  # Appends a single blank line to the end
        parser.exit()


class _ListSuite(_StopAction):
    def __call__(self, parser, namespace, values, option_string=None):
        from talos.config import suites_conf
        print('Available suites:')
        conf = suites_conf()
        max_suite_name = max([len(s) for s in conf])
        pattern = " %%-%ds (%%s)" % max_suite_name
        for name in conf:
            print(pattern % (name, ':'.join(conf[name]['tests'])))
        print
        parser.exit()


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    if not mach_interface:
        add_arg('-e', '--executablePath', required=True, dest="browser_path",
                help="path to executable we are testing")
    add_arg('-t', '--title', default='qm-pxp01',
            help="Title of the test run")
    add_arg('--branchName', dest="branch_name", default='',
            help="Name of the branch we are testing on")
    add_arg('--browserWait', dest='browser_wait', default=5, type=int,
            help="Amount of time allowed for the browser to cleanly close")
    add_arg('-a', '--activeTests',
            help="List of tests to run, separated by ':' (ex. damp:cart)")
    add_arg('--suite',
            help="Suite to use (instead of --activeTests)")
    add_arg('--disable-e10s', dest='e10s',
            action='store_false', default=True,
            help="disable e10s")
    add_arg('--noChrome', action='store_true',
            help="do not run tests as chrome")
    add_arg('--rss', action='store_true',
            help="Collect RSS counters from pageloader instead of the"
                 " operating system")
    add_arg('--mainthread', action='store_true',
            help="Collect mainthread IO data from the browser by setting"
                 " an environment variable")
    add_arg("--mozAfterPaint", action='store_true', dest="tpmozafterpaint",
            help="wait for MozAfterPaint event before recording the time")
    add_arg('--spsProfile', action="store_true", dest="sps_profile",
            help="Profile the run and output the results in $MOZ_UPLOAD_DIR")
    add_arg('--spsProfileInterval', dest='sps_profile_interval', type=int,
            help="How frequently to take samples (ms)")
    add_arg('--spsProfileEntries', dest="sps_profile_entries", type=int,
            help="How many samples to take with the profiler")
    add_arg('--extension', dest='extensions', action='append',
            default=['${talos}/talos-powers/talos-powers-signed.xpi',
                     '${talos}/pageloader/pageloader-signed.xpi'],
            help="Extension to install while running")
    add_arg('--fast', action='store_true',
            help="Run tp tests as tp_fast")
    add_arg('--symbolsPath', dest='symbols_path',
            help="Path to the symbols for the build we are testing")
    add_arg('--xperf_path',
            help="Path to windows performance tool xperf.exe")
    add_arg('--test_timeout', type=int, default=1200,
            help="Time to wait for the browser to output to the log file")
    add_arg('--errorFile', dest='error_filename',
            default=os.path.abspath('browser_failures.txt'),
            help="Filename to store the errors found during the test."
                 " Currently used for xperf only.")
    add_arg('--noShutdown', dest='shutdown', action='store_true',
            help="Record time browser takes to shutdown after testing")
    add_arg('--setpref', action='append', default=[], dest="extraPrefs",
            metavar="PREF=VALUE",
            help="defines an extra user preference")
    add_arg('--webServer', dest='webserver',
            help="DEPRECATED")
    if not mach_interface:
        add_arg('--develop', action='store_true', default=False,
                help="useful for running tests on a developer machine."
                     " Doesn't upload to the graph servers.")
    add_arg("--cycles", type=int,
            help="number of browser cycles to run")
    add_arg("--tpmanifest",
            help="manifest file to test")
    add_arg('--tpcycles', type=int,
            help="number of pageloader cycles to run")
    add_arg('--tptimeout', type=int,
            help='number of milliseconds to wait for a load event after'
                 ' calling loadURI before timing out')
    add_arg('--tppagecycles', type=int,
            help='number of pageloader cycles to run for each page in'
                 ' the manifest')
    add_arg('--tpdelay', type=int,
            help="length of the pageloader delay")
    add_arg('--sourcestamp',
            help='Specify the hg revision or sourcestamp for the changeset'
                 ' we are testing.  This will use the value found in'
                 ' application.ini if it is not specified.')
    add_arg('--repository',
            help='Specify the url for the repository we are testing. '
                 'This will use the value found in application.ini if'
                 ' it is not specified.')
    add_arg('--framework',
            help='Will post to the specified framework for Perfherder. '
                 'Default "talos".  Used primarily for experiments on '
                 'new platforms')
    add_arg('--print-tests', action=_ListTests,
            help="print available tests")
    add_arg('--print-suites', action=_ListSuite,
            help="list available suites")

    add_logging_group(parser)
    return parser


def parse_args(argv=None):
    parser = create_parser()
    return parser.parse_args(argv)
