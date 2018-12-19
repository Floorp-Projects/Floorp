# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import argparse
import os

from mozlog.commandline import add_logging_group


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-t', '--test', required=True, dest='test',
            help="name of raptor test to run")
    add_arg('--app', default='firefox', dest='app',
            help="name of the application we are testing (default: firefox)",
            choices=['firefox', 'chrome', 'geckoview'])
    add_arg('-b', '--binary', dest='binary',
            help="path to the browser executable that we are testing")
    add_arg('--host', dest='host',
            help="Hostname from which to serve urls, defaults to 127.0.0.1.",
            default='127.0.0.1')
    add_arg('--power-test', dest="power_test", action="store_true",
            help="Use Raptor to measure power usage. Currently supported only when "
            "--host specified for geckoview.")
    add_arg('--is-release-build', dest="is_release_build", default=False,
            action='store_true',
            help="Whether the build is a release build which requires work arounds "
            "using MOZ_DISABLE_NONLOCAL_CONNECTIONS to support installing unsigned "
            "webextensions. Defaults to False.")
    add_arg('--geckoProfile', action="store_true", dest="gecko_profile",
            help=argparse.SUPPRESS)
    add_arg('--geckoProfileInterval', dest='gecko_profile_interval', type=float,
            help=argparse.SUPPRESS)
    add_arg('--geckoProfileEntries', dest="gecko_profile_entries", type=int,
            help=argparse.SUPPRESS)
    add_arg('--gecko-profile', action="store_true", dest="gecko_profile",
            help="Profile the run and output the results in $MOZ_UPLOAD_DIR. "
            "After talos is finished, perf-html.io will be launched in Firefox so you "
            "can analyze the local profiles. To disable auto-launching of perf-html.io "
            "set the DISABLE_PROFILE_LAUNCH=1 env var.")
    add_arg('--gecko-profile-interval', dest='gecko_profile_interval', type=float,
            help="How frequently to take samples (milliseconds)")
    add_arg('--gecko-profile-entries', dest="gecko_profile_entries", type=int,
            help="How many samples to take with the profiler")
    add_arg('--symbolsPath', dest='symbols_path',
            help="Path to the symbols for the build we are testing")
    add_arg('--page-cycles', dest="page_cycles", type=int,
            help="How many times to repeat loading the test page (for page load tests); "
                 "for benchmark tests this is how many times the benchmark test will be run")
    add_arg('--page-timeout', dest="page_timeout", type=int,
            help="How long to wait (ms) for one page_cycle to complete, before timing out")
    add_arg('--print-tests', action=_PrintTests,
            help="Print all available Raptor tests")
    add_arg('--debug-mode', dest="debug_mode", action="store_true",
            help="Run Raptor in debug mode (open browser console, limited page-cycles, etc.)")
    if not mach_interface:
        add_arg('--run-local', dest="run_local", default=False, action="store_true",
                help="Flag that indicates if raptor is running locally or in production")
        add_arg('--obj-path', dest="obj_path", default=None,
                help="Browser build obj_path (received when running in production)")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)
    if args.binary is None:
        parser.error("--binary is required!")

    # if running on a desktop browser make sure the binary exists
    if args.app != "geckoview":
        if not os.path.isfile(args.binary):
            parser.error("{binary} does not exist!".format(**ctx))

    # if geckoProfile specified but not running on Firefox, not supported
    if args.gecko_profile is True and args.app != "firefox":
        parser.error("Gecko profiling is only supported when running raptor on Firefox!")

    # if --power-test specified, must be on geckview with --host specified.
    if args.power_test:
        if args.app != "geckoview" or args.host in ('localhost', '127.0.0.1'):
            parser.error("Power test is only supported when running raptor on Geckoview "
                         "when host is specified!")


def parse_args(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    verify_options(parser, args)
    return args


class _StopAction(argparse.Action):
    def __init__(self, option_strings, dest=argparse.SUPPRESS,
                 default=argparse.SUPPRESS, help=None):
        super(_StopAction, self).__init__(
            option_strings=option_strings,
            dest=dest,
            default=default,
            nargs=0,
            help=help)


class _PrintTests(_StopAction):
    def __call__(self, parser, namespace, values, option_string=None):
        from manifestparser import TestManifest

        here = os.path.abspath(os.path.dirname(__file__))
        raptor_ini = os.path.join(here, 'raptor.ini')

        for _app in ["firefox", "chrome", "geckoview", "chrome-android"]:
            test_manifest = TestManifest([raptor_ini], strict=False)
            info = {"app": _app}
            available_tests = test_manifest.active_tests(exists=False,
                                                         disabled=False,
                                                         filters=[self.filter_app],
                                                         **info)
            if len(available_tests) == 0:
                # none for that app, skip to next
                continue

            # print in readable format
            if _app == "firefox":
                title = "\nRaptor Tests Available for %s" % self.get_long_name(_app)
            else:
                title = "\nRaptor Tests Available for %s (--app=%s)" \
                    % (self.get_long_name(_app), _app)

            print(title)
            print("=" * (len(title) - 1))

            # build the list of tests for this app
            test_list = {}

            for next_test in available_tests:
                if next_test.get("name", None) is None:
                    # no test name, skip it
                    continue

                suite = os.path.basename(next_test['manifest'])[:-4]
                if suite not in test_list:
                    test_list[suite] = {'type': None, 'subtests': []}

                # for page-load tests we want to list every subtest, so we
                # can see which pages are available in which tp6-* sets
                if next_test.get("type", None) is not None:
                    test_list[suite]['type'] = next_test['type']
                    if next_test['type'] == "pageload":
                        test_list[suite]['subtests'].append(next_test['name'])

            # print the list in a nice readable format
            for key in sorted(test_list.iterkeys()):
                print("\n%s" % key)
                print("  type: %s" % test_list[key]['type'])
                if len(test_list[key]['subtests']) != 0:
                    print("  subtests:")
                    for _sub in sorted(test_list[key]['subtests']):
                        print("    %s" % _sub)

        print("\nDone.")
        # exit Raptor
        parser.exit()

    def get_long_name(self, app):
        if app == "firefox":
            return "Firefox Desktop"
        elif app == "chrome":
            return "Google Chrome Desktop"
        elif app == "geckoview":
            return "Firefox Geckoview on Android"
        elif app == "chrome-android":
            return "Google Chrome on Android"

    def filter_app(self, tests, values):
        for test in tests:
            if values["app"] in test['apps']:
                yield test
