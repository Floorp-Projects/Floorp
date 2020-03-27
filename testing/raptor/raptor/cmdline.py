# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import argparse
import os
import platform

from mozlog.commandline import add_logging_group

(FIREFOX,
 CHROME,
 CHROMIUM) = DESKTOP_APPS = ["firefox", "chrome", "chromium"]
(FENNEC,
 GECKOVIEW,
 REFBROW,
 FENIX,
 CHROME_ANDROID) = FIREFOX_ANDROID_APPS = [
    "fennec", "geckoview", "refbrow", "fenix", "chrome-m"
]

CHROMIUM_DISTROS = [CHROME, CHROMIUM]
APPS = {
    FIREFOX: {
        "long_name": "Firefox Desktop"},
    CHROME: {
        "long_name": "Google Chrome Desktop"},
    CHROMIUM: {
        "long_name": "Google Chromium Desktop"},
    FENNEC: {
        "long_name": "Firefox Fennec on Android"},
    GECKOVIEW: {
        "long_name": "Firefox GeckoView on Android",
        "default_activity": "org.mozilla.geckoview_example.GeckoViewActivity",
        "default_intent": "android.intent.action.MAIN"},
    REFBROW: {
        "long_name": "Firefox Android Components Reference Browser",
        "default_activity": "org.mozilla.reference.browser.BrowserTestActivity",
        "default_intent": "android.intent.action.MAIN"},
    FENIX: {
        "long_name": "Firefox Android Fenix Browser",
        "default_activity": "org.mozilla.fenix.IntentReceiverActivity",
        "default_intent": "android.intent.action.VIEW"},
    CHROME_ANDROID: {
        "long_name": "Google Chrome on Android",
        "default_activity": "com.android.chrome/com.google.android.apps.chrome.Main",
        "default_intent": "android.intent.action.VIEW"}
}
INTEGRATED_APPS = list(APPS.keys())


def print_all_activities():
    all_activities = []
    for next_app in APPS:
        if APPS[next_app].get('default_activity', None) is not None:
            _activity = "%s:%s" % (next_app, APPS[next_app]['default_activity'])
            all_activities.append(_activity)
    return all_activities


def print_all_intents():
    all_intents = []
    for next_app in APPS:
        if APPS[next_app].get('default_intent', None) is not None:
            _intent = "%s:%s" % (next_app, APPS[next_app]['default_intent'])
            all_intents.append(_intent)
    return all_intents


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-t', '--test', required=True, dest='test',
            help="Name of Raptor test to run (can be a top-level suite name i.e. "
            "'--test raptor-speedometer','--test raptor-tp6-1', or for page-load "
            "tests a suite sub-test i.e. '--test raptor-tp6-google-firefox')")
    add_arg('--app', default='firefox', dest='app',
            help="Name of the application we are testing (default: firefox)",
            choices=APPS.keys())
    add_arg('-b', '--binary', dest='binary',
            help="path to the browser executable that we are testing")
    add_arg('-a', '--activity', dest='activity', default=None,
            help="Name of Android activity used to launch the Android app."
            "i.e.: %s" % print_all_activities())
    add_arg('-i', '--intent', dest='intent', default=None,
            help="Name of Android intent action used to launch the Android app."
            "i.e.: %s" % print_all_intents())
    add_arg('--host', dest='host',
            help="Hostname from which to serve URLs; defaults to 127.0.0.1. "
            "The value HOST_IP will cause the value of host to be "
            "loaded from the environment variable HOST_IP.",
            default='127.0.0.1')
    add_arg('--power-test', dest="power_test", action="store_true",
            help="Use Raptor to measure power usage on Android browsers (Geckoview Example, "
            "Fenix, Refbrow, and Fennec) as well as on Intel-based MacOS machines that have "
            "Intel Power Gadget installed.")
    add_arg('--memory-test', dest="memory_test", action="store_true",
            help="Use Raptor to measure memory usage.")
    add_arg('--cpu-test', dest="cpu_test", action="store_true",
            help="Use Raptor to measure CPU usage. Currently supported for Android only.")
    add_arg('--is-release-build', dest="is_release_build", default=False,
            action='store_true',
            help="Whether the build is a release build which requires workarounds "
            "using MOZ_DISABLE_NONLOCAL_CONNECTIONS to support installing unsigned "
            "webextensions. Defaults to False.")
    add_arg('--geckoProfile', action="store_true", dest="gecko_profile",
            help=argparse.SUPPRESS)
    add_arg('--geckoProfileInterval', dest='gecko_profile_interval', type=float,
            help=argparse.SUPPRESS)
    add_arg('--geckoProfileEntries', dest="gecko_profile_entries", type=int,
            help=argparse.SUPPRESS)
    add_arg('--gecko-profile', action="store_true", dest="gecko_profile",
            help="Profile the run and out-put the results in $MOZ_UPLOAD_DIR. "
            "After talos is finished, profiler.firefox.com will be launched in Firefox "
            "so you can analyze the local profiles. To disable auto-launching of "
            "profiler.firefox.com, set the DISABLE_PROFILE_LAUNCH=1 env var.")
    add_arg('--gecko-profile-entries', dest="gecko_profile_entries", type=int,
            help='How many samples to take with the profiler')
    add_arg('--gecko-profile-interval', dest='gecko_profile_interval', type=int,
            help='How frequently to take samples (milliseconds)')
    add_arg('--gecko-profile-thread', dest='gecko_profile_threads', action='append',
            help='Name of the extra thread to be profiled')
    add_arg('--symbolsPath', dest='symbols_path',
            help="Path to the symbols for the build we are testing")
    add_arg('--page-cycles', dest="page_cycles", type=int,
            help="How many times to repeat loading the test page (for page load tests); "
                 "for benchmark tests, this is how many times the benchmark test will be run")
    add_arg('--page-timeout', dest="page_timeout", type=int,
            help="How long to wait (ms) for one page_cycle to complete, before timing out")
    add_arg('--post-startup-delay',
            dest='post_startup_delay',
            type=int,
            default=30000,
            help='How long to wait (ms) after browser start-up before starting the tests')
    add_arg('--browser-cycles', dest="browser_cycles", type=int,
            help="The number of times a cold load test is repeated (for cold load tests only, "
            "where the browser is shutdown and restarted between test iterations)")
    add_arg('--test-url-params', dest='test_url_params',
            help="Parameters to add to the test_url query string")
    add_arg('--print-tests', action=_PrintTests,
            help="Print all available Raptor tests")
    add_arg('--debug-mode', dest="debug_mode", action="store_true",
            help="Run Raptor in debug mode (open browser console, limited page-cycles, etc.)")
    add_arg('--disable-e10s', dest="e10s", action="store_false", default=True,
            help="Run without multiple processes (e10s).")
    add_arg('--enable-webrender', dest="enable_webrender", action="store_true", default=False,
            help="Enable the WebRender compositor in Gecko.")
    add_arg('--no-conditioned-profile', dest="no_conditioned_profile", action="store_true",
            default=False, help="Run Raptor tests without a conditioned profile.")
    add_arg('--device-name', dest="device_name", default=None,
            type=str, help="Device name of mobile device.")
    add_arg('--enable-fission', dest="enable_fission", action="store_true", default=False,
            help="Enable Fission (site isolation) in Gecko.")
    add_arg('--setpref', dest="extra_prefs", action="append", default=[],
            help="A preference to set. Must be a key-value pair separated by a ':'.")
    if not mach_interface:
        add_arg('--run-local', dest="run_local", default=False, action="store_true",
                help="Flag which indicates if Raptor is running locally or in production")
        add_arg('--obj-path', dest="obj_path", default=None,
                help="Browser-build obj_path (received when running in production)")
    add_arg('--noinstall', dest="noinstall", default=False, action="store_true",
            help="Flag which indicates if Raptor should not offer to install Android APK.")
    add_arg('--installerpath', dest="installerpath", default=None, type=str,
            help="Location where Android browser APK was extracted to before installation.")

    # for browsertime jobs, cold page load is determined by a '--cold' cmd line argument
    add_arg('--cold', dest="cold", action="store_true",
            help="Enable cold page-load for browsertime tp6")
    # Arguments for invoking browsertime.
    add_arg('--browsertime', dest='browsertime', default=False, action="store_true",
            help="Whether to use browsertime to execute pageload tests")
    add_arg('--browsertime-node', dest='browsertime_node',
            help="path to Node.js executable")
    add_arg('--browsertime-browsertimejs', dest='browsertime_browsertimejs',
            help="path to browsertime.js script")
    add_arg('--browsertime-chromedriver', dest='browsertime_chromedriver',
            help="path to chromedriver executable")
    add_arg('--browsertime-video', dest='browsertime_video',
            help="records the viewport", default=False, action="store_true")
    add_arg('--browsertime-ffmpeg', dest='browsertime_ffmpeg',
            help="path to ffmpeg executable (for `--video=true`)")
    add_arg('--browsertime-geckodriver', dest='browsertime_geckodriver',
            help="path to geckodriver executable")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)
    if args.binary is None and args.app != "chrome-m":
        parser.error("--binary is required!")

    # if running chrome android tests, make sure it's on browsertime and
    # that the chromedriver path was provided
    if args.app == "chrome-m":
        if not args.browsertime:
            parser.error("--browsertime is required to run android chrome tests")
        if not args.browsertime_chromedriver:
            parser.error(
                "--browsertime-chromedriver path is required for android chrome tests"
            )

    # if running on a desktop browser make sure the binary exists
    if args.app in DESKTOP_APPS:
        if not os.path.isfile(args.binary):
            parser.error("{binary} does not exist!".format(**ctx))

    # if geckoProfile specified but running on Chrom[e|ium], not supported
    if args.gecko_profile and args.app in CHROMIUM_DISTROS:
        parser.error("Gecko profiling is not supported on Chrome/Chromium!")

    if args.power_test:
        if args.app not in ["fennec", "geckoview", "refbrow", "fenix"]:
            if platform.system().lower() not in ('darwin',):
                parser.error("Power tests are only available on MacOS desktop machines or "
                             "Firefox android browers. App requested: %s. Platform "
                             "detected: %s." % (args.app, platform.system().lower()))

    if args.cpu_test:
        if args.app not in ["fennec", "geckoview", "refbrow", "fenix"]:
            parser.error("CPU test is only supported when running Raptor on Firefox Android "
                         "browsers!")

    if args.memory_test:
        if args.app not in ["fennec", "geckoview", "refbrow", "fenix"]:
            parser.error("Memory test is only supported when running Raptor on Firefox Android "
                         "browsers!")

    # if --enable-webrender specified, must be on desktop firefox or geckoview-based browser.
    if args.enable_webrender:
        if args.app not in ["firefox", "geckoview", "refbrow", "fenix"]:
            parser.error("WebRender is only supported when running Raptor on Firefox Desktop "
                         "or GeckoView-based Android browsers!")

    # if running on geckoview/refbrow/fenix, we need an activity and intent
    if args.app in ["geckoview", "refbrow", "fenix"]:
        if not args.activity:
            # if we have a default activity specified in APPS above, use that
            if APPS[args.app].get("default_activity", None) is not None:
                args.activity = APPS[args.app]['default_activity']
            else:
                # otherwise fail out
                parser.error("--activity command-line argument is required!")
        if not args.intent:
            # if we have a default intent specified in APPS above, use that
            if APPS[args.app].get("default_intent", None) is not None:
                args.intent = APPS[args.app]['default_intent']
            else:
                # otherwise fail out
                parser.error("--intent command-line argument is required!")


def parse_args(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    if args.host == 'HOST_IP':
        args.host = os.environ['HOST_IP']
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
    def __init__(self, integrated_apps=INTEGRATED_APPS, *args, **kwargs):
        super(_PrintTests, self).__init__(*args, **kwargs)
        self.integrated_apps = integrated_apps

    def __call__(self, parser, namespace, values, option_string=None):
        from manifestparser import TestManifest

        here = os.path.abspath(os.path.dirname(__file__))
        raptor_ini = os.path.join(here, 'raptor.ini')

        for _app in self.integrated_apps:
            test_manifest = TestManifest([raptor_ini], strict=False)
            info = {"app": _app}
            available_tests = test_manifest.active_tests(exists=False,
                                                         disabled=False,
                                                         filters=[self.filter_app],
                                                         **info)
            if len(available_tests) == 0:
                # none for that app; skip to next
                continue

            # print in readable format
            if _app == "firefox":
                title = "\nRaptor Tests Available for %s" % APPS[_app]['long_name']
            else:
                title = "\nRaptor Tests Available for %s (--app=%s)" \
                    % (APPS[_app]['long_name'], _app)

            print(title)
            print("=" * (len(title) - 1))

            # build the list of tests for this app
            test_list = {}

            for next_test in available_tests:
                if next_test.get("name", None) is None:
                    # no test name; skip it
                    continue

                suite = os.path.basename(next_test['manifest'])[:-4]
                if suite not in test_list:
                    test_list[suite] = {'type': None, 'subtests': []}

                # for page-load tests, we want to list every subtest, so we
                # can see which pages are available in which tp6-* sets
                if next_test.get("type", None) is not None:
                    test_list[suite]['type'] = next_test['type']
                    if next_test['type'] == "pageload":
                        subtest = next_test['name']
                        measure = next_test.get("measure")
                        if measure is not None:
                            subtest = "{0} ({1})".format(subtest, measure)
                        test_list[suite]['subtests'].append(subtest)

            # print the list in a nice, readable format
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

    def filter_app(self, tests, values):
        for test in tests:
            if values["app"] in test['apps']:
                yield test
