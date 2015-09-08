# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import copy

from talos import utils, test


class _ListTests(argparse.Action):
    def __init__(self, option_strings, dest=argparse.SUPPRESS,
                 default=argparse.SUPPRESS, help=None):
        super(_ListTests, self).__init__(
            option_strings=option_strings,
            dest=dest,
            default=default,
            nargs=0,
            help=help)

    def __call__(self, parser, namespace, values, option_string=None):
        print 'Available tests:'
        print '================\n'
        test_class_names = [
            (test_class.name(), test_class.description())
            for test_class in test.test_dict().itervalues()
        ]
        test_class_names.sort()
        for name, description in test_class_names:
            print name
            print '-'*len(name)
            print description
            print  # Appends a single blank line to the end
        parser.exit()


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-e', '--executablePath', required=True, dest="browser_path",
            help="path to executable we are testing")
    add_arg('-t', '--title', default='qm-pxp01',
            help="Title of the test run")
    add_arg('--branchName', dest="branch_name", default='',
            help="Name of the branch we are testing on")
    add_arg('--browserWait', dest='browser_wait', default=5, type=int,
            help="Amount of time allowed for the browser to cleanly close")
    add_arg('-a', '--activeTests', required=True,
            help="List of tests to run, separated by ':' (ex. damp:cart)")
    add_arg('--e10s', action='store_true',
            help="enable e10s")
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
            default=['${talos}/talos-powers', '${talos}/pageloader'],
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
    add_arg('--setPref', action='append', default=[], dest="extraPrefs",
            metavar="PREF=VALUE",
            help="defines an extra user preference")
    add_arg('--webServer', dest='webserver',
            help="address of the webserver hosting the talos files")
    add_arg('--develop', action='store_true', default=False,
            help="useful for running tests on a developer machine."
                 " Creates a local webserver and doesn't upload to the"
                 " graph servers.")
    add_arg('--responsiveness', action='store_true',
            help="turn on responsiveness collection")
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
    add_arg('--print-tests', action=_ListTests,
            help="print available tests")
    add_arg('--debug', action='store_true',
            help='show debug information')

    return parser.parse_args(argv)


class ConfigurationError(Exception):
    pass


DEFAULTS = dict(
    # args to pass to browser
    extra_args='',
    buildid='testbuildid',
    init_url='getInfo.html',
    env={'NO_EM_RESTART': '1'},
    # base data for all tests
    basetest=dict(
        cycles=1,
        test_name_extension='',
        profile_path='${talos}/base_profile',
        responsiveness=False,
        e10s=False,
        sps_profile=False,
        sps_profile_interval=1,
        sps_profile_entries=100000,
        resolution=1,
        rss=False,
        mainthread=False,
        shutdown=False,
        timeout=3600,
        tpchrome=True,
        tpcycles=10,
        tpmozafterpaint=False,
        tpdisable_e10s=False,
        tpnoisy=True,
        tppagecycles=1,
        tploadnocache=False,
        tpscrolltest=False,
        tprender=False,
        win_counters=[],
        w7_counters=[],
        linux_counters=[],
        mac_counters=[],
        xperf_counters=[],
        setup=None,
        cleanup=None,
    ),
    # default preferences to run with
    # these are updated with --extraPrefs from the commandline
    # for extension scopes, see
    # see https://developer.mozilla.org/en/Installing_extensions
    preferences={
        'app.update.enabled': False,
        'browser.addon-watch.interval': -1,  # Deactivate add-on watching
        'browser.aboutHomeSnippets.updateUrl':
            'https://127.0.0.1/about-dummy/',
        'browser.bookmarks.max_backups': 0,
        'browser.cache.disk.smart_size.enabled': False,
        'browser.cache.disk.smart_size.first_run': False,
        'browser.chrome.dynamictoolbar': False,
        'browser.dom.window.dump.enabled': True,
        'browser.EULA.override': True,
        'browser.link.open_newwindow': 2,
        'browser.reader.detectedFirstArticle': True,
        'browser.shell.checkDefaultBrowser': False,
        'browser.warnOnQuit': False,
        'browser.tabs.remote.autostart': False,
        'dom.allow_scripts_to_close_windows': True,
        'dom.disable_open_during_load': False,
        'dom.disable_window_flip': True,
        'dom.disable_window_move_resize': True,
        'dom.max_chrome_script_run_time': 0,
        'dom.max_script_run_time': 0,
        'extensions.autoDisableScopes': 10,
        'extensions.checkCompatibility': False,
        'extensions.enabledScopes': 5,
        'extensions.update.notifyUser': False,
        'xpinstall.signatures.required': False,
        'hangmonitor.timeout': 0,
        'network.proxy.http': 'localhost',
        'network.proxy.http_port': 80,
        'network.proxy.type': 1,
        'security.enable_java': False,
        'security.fileuri.strict_origin_policy': False,
        'toolkit.telemetry.prompted': 999,
        'toolkit.telemetry.notifiedOptOut': 999,
        'dom.send_after_paint_to_content': True,
        'security.turn_off_all_security_so_that_viruses_can_'
        'take_over_this_computer': True,
        'browser.newtabpage.directory.source':
            '${webserver}/directoryLinks.json',
        'browser.newtabpage.directory.ping': '',
        'browser.newtabpage.introShown': True,
        'browser.safebrowsing.provider.google.gethashURL':
            'http://127.0.0.1/safebrowsing-dummy/gethash',
        'browser.safebrowsing.provider.google.updateURL':
            'http://127.0.0.1/safebrowsing-dummy/update',
        'browser.safebrowsing.provider.mozilla.gethashURL':
            'http://127.0.0.1/safebrowsing-dummy/gethash',
        'browser.safebrowsing.provider.mozilla.updateURL':
            'http://127.0.0.1/safebrowsing-dummy/update',
        'privacy.trackingprotection.introURL':
            'http://127.0.0.1/trackingprotection/tour',
        'browser.safebrowsing.enabled': False,
        'browser.safebrowsing.malware.enabled': False,
        'browser.search.isUS': True,
        'browser.search.countryCode': 'US',
        'browser.selfsupport.url':
            'https://127.0.0.1/selfsupport-dummy/',
        'extensions.update.url':
            'http://127.0.0.1/extensions-dummy/updateURL',
        'extensions.update.background.url':
            'http://127.0.0.1/extensions-dummy/updateBackgroundURL',
        'extensions.blocklist.enabled': False,
        'extensions.blocklist.url':
            'http://127.0.0.1/extensions-dummy/blocklistURL',
        'extensions.hotfix.url':
            'http://127.0.0.1/extensions-dummy/hotfixURL',
        'extensions.update.enabled': False,
        'extensions.webservice.discoverURL':
            'http://127.0.0.1/extensions-dummy/discoveryURL',
        'extensions.getAddons.maxResults': 0,
        'extensions.getAddons.get.url':
            'http://127.0.0.1/extensions-dummy/repositoryGetURL',
        'extensions.getAddons.getWithPerformance.url':
            'http://127.0.0.1/extensions-dummy'
            '/repositoryGetWithPerformanceURL',
        'extensions.getAddons.search.browseURL':
            'http://127.0.0.1/extensions-dummy/repositoryBrowseURL',
        'extensions.getAddons.search.url':
            'http://127.0.0.1/extensions-dummy/repositorySearchURL',
        'plugins.update.url':
            'http://127.0.0.1/plugins-dummy/updateCheckURL',
        'media.gmp-manager.url':
            'http://127.0.0.1/gmpmanager-dummy/update.xml',
        'media.navigator.enabled': True,
        'media.peerconnection.enabled': True,
        'media.navigator.permission.disabled': True,
        'media.capturestream_hints.enabled': True,
        'browser.contentHandlers.types.0.uri': 'http://127.0.0.1/rss?url=%s',
        'browser.contentHandlers.types.1.uri': 'http://127.0.0.1/rss?url=%s',
        'browser.contentHandlers.types.2.uri': 'http://127.0.0.1/rss?url=%s',
        'browser.contentHandlers.types.3.uri': 'http://127.0.0.1/rss?url=%s',
        'browser.contentHandlers.types.4.uri': 'http://127.0.0.1/rss?url=%s',
        'browser.contentHandlers.types.5.uri': 'http://127.0.0.1/rss?url=%s',
        'identity.fxaccounts.auth.uri': 'https://127.0.0.1/fxa-dummy/',
        'datareporting.healthreport.about.reportUrl':
            'http://127.0.0.1/abouthealthreport/',
        'datareporting.healthreport.documentServerURI':
            'http://127.0.0.1/healthreport/',
        'datareporting.policy.dataSubmissionPolicyBypassNotification': True,
        'general.useragent.updates.enabled': False,
        'browser.webapps.checkForUpdates': 0,
        'browser.search.geoSpecificDefaults': False,
        'browser.snippets.enabled': False,
        'browser.snippets.syncPromo.enabled': False,
        'toolkit.telemetry.server': 'https://127.0.0.1/telemetry-dummy/',
        'experiments.manifest.uri':
            'https://127.0.0.1/experiments-dummy/manifest',
        'network.http.speculative-parallel-limit': 0,
        'browser.displayedE10SPrompt': 9999,
        'browser.displayedE10SPrompt.1': 9999,
        'browser.displayedE10SPrompt.2': 9999,
        'browser.displayedE10SPrompt.3': 9999,
        'browser.displayedE10SPrompt.4': 9999,
        'browser.displayedE10SPrompt.5': 9999,
        'app.update.badge': False,
        'lightweightThemes.selectedThemeID': "",
        'devtools.webide.widget.enabled': False,
        'devtools.webide.widget.inNavbarByDefault': False,
        'devtools.chrome.enabled': False,
        'devtools.debugger.remote-enabled': False,
        'devtools.theme': "light",
        'devtools.timeline.enabled': False,
        'identity.fxaccounts.migrateToDevEdition': False
    }
)


# keys to generated self.config that are global overrides to tests
GLOBAL_OVERRIDES = (
    'cycles',
    'responsiveness',
    'sps_profile',
    'sps_profile_interval',
    'sps_profile_entries',
    'rss',
    'mainthread',
    'shutdown',
    'tpcycles',
    'tpdelay',
    'tppagecycles',
    'tpmanifest',
    'tptimeout',
    'tpmozafterpaint',
)


CONF_VALIDATORS = []


def validator(func):
    """
    A decorator that register configuration validators to execute against the
    configuration.

    They will be executed in the order of declaration.
    """
    CONF_VALIDATORS.append(func)
    return func


def convert_url(config, url):
    webserver = config['webserver']
    if not webserver:
        return url

    if '://' in url:
        # assume a fully qualified url
        return url

    if '.html' in url:
        url = 'http://%s/%s' % (webserver, url)

    return url


@validator
def fix_xperf(config):
    # BBB: remove doubly-quoted xperf values from command line
    # (needed for buildbot)
    # https://bugzilla.mozilla.org/show_bug.cgi?id=704654#c43
    if config['xperf_path']:
        xperf_path = config['xperf_path']
        quotes = ('"', "'")
        for quote in quotes:
            if xperf_path.startswith(quote) and xperf_path.endswith(quote):
                config['xperf_path'] = xperf_path[1:-1]
                break
        if not os.path.exists(config['xperf_path']):
            raise ConfigurationError(
                "xperf.exe cannot be found at the path specified")


@validator
def check_webserver(config):
    if config['develop'] and not config['webserver']:
        config['webserver'] = 'localhost:15707'


@validator
def update_prefs(config):
    # if e10s is enabled, set prefs accordingly
    if config['e10s']:
        config['preferences']['browser.tabs.remote.autostart'] = True
    else:
        config['preferences']['browser.tabs.remote.autostart'] = False
        config['preferences']['browser.tabs.remote.autostart.1'] = False
        config['preferences']['browser.tabs.remote.autostart.2'] = False

    # update prefs from command line
    prefs = config.pop('extraPrefs')
    if prefs:
        for arg in prefs:
            k, v = arg.split('=', 1)
            config['preferences'][k] = utils.parse_pref(v)


@validator
def fix_init_url(config):
    if 'init_url' in config:
        config['init_url'] = convert_url(config, config['init_url'])


def get_counters(config):
    counters = set()
    if config['rss']:
        counters.add('Main_RSS')
    return counters


def get_active_tests(config):
    activeTests = config.pop('activeTests').strip().split(':')

    # temporary hack for now until we have e10s running on all tests
    if config['e10s'] and not config['develop']:
        for testname in ('sessionrestore',
                         'sessionrestore_no_auto_restore'):
            if testname in activeTests:
                print "%s is unsupported on e10s, removing from list of " \
                    "tests to run" % testname
                activeTests.remove(testname)
    # ensure tests are available
    availableTests = test.test_dict()
    if not set(activeTests).issubset(availableTests):
        missing = [i for i in activeTests
                   if i not in availableTests]
        raise ConfigurationError("No definition found for test(s): %s"
                                 % missing)
    return activeTests


def get_global_overrides(config):
    global_overrides = {}
    for key in GLOBAL_OVERRIDES:
        # get global overrides for all tests
        value = config.pop(key)
        if value is not None:
            global_overrides[key] = value

    # add noChrome to global overrides (HACK)
    noChrome = config.pop('noChrome')
    if noChrome:
        global_overrides['tpchrome'] = False

    # HACK: currently xperf tests post results to graph server and
    # we want to ensure we don't publish shutdown numbers
    # This is also hacked because "--noShutdown -> shutdown:True"
    if config['xperf_path']:
        global_overrides['shutdown'] = False
    return global_overrides


def build_manifest(config, manifestName):
    # read manifest lines
    with open(manifestName, 'r') as fHandle:
        manifestLines = fHandle.readlines()

    # write modified manifest lines
    with open(manifestName + '.develop', 'w') as newHandle:
        for line in manifestLines:
            newHandle.write(line.replace('localhost',
                                         config['webserver']))

    newManifestName = manifestName + '.develop'

    # return new manifest
    return newManifestName


def get_test(config, global_overrides, counters, test_instance):
    mozAfterPaint = getattr(test_instance, 'tpmozafterpaint', None)

    # add test_name_extension to config
    if mozAfterPaint:
        test_instance.test_name_extension = '_paint'

    test_instance.update(**global_overrides)

    # update original value of mozAfterPaint, this could be 'false',
    # so check for None
    if mozAfterPaint is not None:
        test_instance.tpmozafterpaint = mozAfterPaint

    # fix up url
    url = getattr(test_instance, 'url', None)
    if url:
        test_instance.url = utils.interpolate(convert_url(config, url))

    # fix up tpmanifest
    tpmanifest = getattr(test_instance, 'tpmanifest', None)
    if tpmanifest and config.get('develop'):
        test_instance.tpmanifest = \
            build_manifest(config, utils.interpolate(tpmanifest))

    # add any counters
    if counters:
        keys = ('linux_counters', 'mac_counters',
                'win_counters', 'w7_counters', 'xperf_counters')
        for key in keys:
            if key not in test_instance.keys:
                # only populate attributes that will be output
                continue
            if not isinstance(getattr(test_instance, key, None), list):
                setattr(test_instance, key, [])
            _counters = getattr(test_instance, key)
            _counters.extend([counter for counter in counters
                              if counter not in _counters])
    return dict(test_instance.items())


@validator
def tests(config):
    counters = get_counters(config)
    global_overrides = get_global_overrides(config)
    activeTests = get_active_tests(config)
    test_dict = test.test_dict()

    tests = []
    for test_name in activeTests:
        test_class = test_dict[test_name]
        tests.append(get_test(config, global_overrides, counters,
                              test_class()))
    config['tests'] = tests


def get_browser_config(config):
    required = ('preferences', 'extensions', 'browser_path', 'browser_wait',
                'extra_args', 'buildid', 'env', 'init_url')
    optional = {'bcontroller_config': '${talos}/bcontroller.json',
                'branch_name': '',
                'child_process': 'plugin-container',
                'develop': False,
                'e10s': False,
                'process': '',
                'repository': None,
                'sourcestamp': None,
                'symbols_path': None,
                'test_name_extension': '',
                'test_timeout': 1200,
                'webserver': '',
                'xperf_path': None,
                'error_filename': None,
                }
    browser_config = dict(title=config['title'])
    browser_config.update(dict([(i, config[i]) for i in required]))
    browser_config.update(dict([(i, config.get(i, j))
                          for i, j in optional.items()]))
    return browser_config


def get_config(argv=None):
    cli_opts = parse_args(argv=argv)
    config = copy.deepcopy(DEFAULTS)
    config.update(cli_opts.__dict__)
    for validate in CONF_VALIDATORS:
        validate(config)
    # remove None Values
    for k, v in config.items():
        if v is None:
            del config[k]
    return config


def get_configs(argv=None):
    config = get_config(argv=argv)
    browser_config = get_browser_config(config)
    return config, browser_config


if __name__ == '__main__':
    cfgs = get_configs()
    print cfgs[0]
    print
    print cfgs[1]
