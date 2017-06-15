import os
from talos import filter, utils

"""
test definitions for Talos
"""

_TESTS = {}  # internal dict of Talos test classes


def register_test():
    """Decorator to register Talos test classes"""
    def wrapper(klass):
        assert issubclass(klass, Test)
        assert klass.name() not in _TESTS

        _TESTS[klass.name()] = klass
        return klass
    return wrapper


def test_dict():
    """Return the dict of the registered test classes"""
    return _TESTS


class Test(object):
    """abstract base class for a Talos test case"""
    cycles = None  # number of cycles
    keys = []
    desktop = True
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    lower_is_better = True
    alert_threshold = 2.0

    @classmethod
    def name(cls):
        return cls.__name__

    @classmethod
    def description(cls):
        if cls.__doc__ is None:
            return "No documentation available yet."
        else:
            doc = cls.__doc__
            description_lines = [i.strip() for i in doc.strip().splitlines()]
            return "\n".join(description_lines)

    def __init__(self, **kw):
        self.update(**kw)

    def update(self, **kw):
        self.__dict__.update(kw)

    def items(self):
        """
        returns a list of 2-tuples
        """
        retval = [('name', self.name())]
        for key in self.keys:
            value = getattr(self, key, None)
            if value is not None:
                retval.append((key, value))
        return retval

    def __str__(self):
        """string form appropriate for YAML output"""
        items = self.items()

        key, value = items.pop(0)
        lines = ["- %s: %s" % (key, value)]
        for key, value in items:
            lines.append('  %s: %s' % (key, value))
        return '\n'.join(lines)


# ts-style startup tests (ts, twinopen, ts_cold, etc)
# The overall test number is calculated by excluding the max opening time
# and taking an average of the remaining numbers.
class TsBase(Test):
    """abstract base class for ts-style tests"""
    keys = [
        'url',
        'url_timestamp',
        'timeout',
        'cycles',
        'shutdown',      # If True, collect data on shutdown (using the value
                         # provided by __startTimestamp/__endTimestamp).
                         # Otherwise, ignore shutdown data
                         # (__startTimestamp/__endTimestamp is still
                         # required but ignored).
        'profile_path',  # The path containing the template profile. This
                         # directory is copied to the temporary profile during
                         # initialization of the test. If some of the files may
                         # be overwritten by Firefox and need to be reinstalled
                         # before each pass, use key |reinstall|
        'gecko_profile',
        'gecko_profile_interval',
        'gecko_profile_entries',
        'gecko_profile_startup',
        'preferences',
        'xperf_counters',
        'xperf_providers',
        'xperf_user_providers',
        'xperf_stackwalk',
        'tpmozafterpaint',
        'extensions',
        'filters',
        'setup',
        'cleanup',
        'webextensions',
        'reinstall',     # A list of files from the profile directory that
                         # should be copied to the temporary profile prior to
                         # running each cycle, to avoid one cycle overwriting
                         # the data used by the next another cycle (may be used
                         # e.g. for sessionstore.js to ensure that all cycles
                         # use the exact same sessionstore.js, rather than a
                         # more recent copy).
    ]


@register_test()
class ts_paint(TsBase):
    """
    Launches tspaint_test.html with the current timestamp in the url,
    waits for [MozAfterPaint and onLoad] to fire, then records the end
    time and calculates the time to startup.
    """
    cycles = 20
    timeout = 150
    gecko_profile_startup = True
    gecko_profile_entries = 10000000
    url = 'startup_test/tspaint_test.html'
    shutdown = False
    xperf_counters = []
    win7_counters = []
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    tpmozafterpaint = True
    rss = False
    mainthread = False
    responsiveness = False
    unit = 'ms'


@register_test()
class ts_paint_webext(ts_paint):
    webextensions = '${talos}/webextensions/dummy/dummy-signed.xpi'
    preferences = {'xpinstall.signatures.required': False}


@register_test()
class sessionrestore(TsBase):
    """
    A start up test measuring the time it takes to load a sessionstore.js file.

    1. Set up Firefox to restore from a given sessionstore.js file.
    2. Launch Firefox.
    3. Measure the delta between firstPaint and sessionRestored.
    """
    extensions = \
        '${talos}/startup_test/sessionrestore/addon/sessionrestore-signed.xpi'
    cycles = 10
    timeout = 1000000
    gecko_profile_startup = True
    gecko_profile_entries = 10000000
    profile_path = '${talos}/startup_test/sessionrestore/profile'
    url = 'startup_test/sessionrestore/index.html'
    shutdown = False
    reinstall = ['sessionstore.js', 'sessionCheckpoints.json']
    # Restore the session
    preferences = {'browser.startup.page': 3}
    unit = 'ms'


@register_test()
class sessionrestore_no_auto_restore(sessionrestore):
    """
    A start up test measuring the time it takes to load a sessionstore.js file.

    1. Set up Firefox to *not* restore automatically from sessionstore.js file.
    2. Launch Firefox.
    3. Measure the delta between firstPaint and sessionRestored.
    """
    # Restore about:home
    preferences = {'browser.startup.page': 1}


@register_test()
class tpaint(TsBase):
    """
    Tests the amount of time it takes the open a new window. This test does
    not include startup time. Multiple test windows are opened in succession,
    results reported are the average amount of time required to create and
    display a window in the running instance of the browser.
    (Measures ctrl-n performance.)
    """
    url = 'file://${talos}/startup_test/tpaint.html?auto=1'
    timeout = 300
    gecko_profile_interval = 1
    gecko_profile_entries = 2000000
    tpmozafterpaint = True
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tresize(TsBase):
    """
    This test does some resize thing.
    """
    extensions = '${talos}/startup_test/tresize/addon/tresize-signed.xpi'
    cycles = 20
    url = 'startup_test/tresize/addon/content/tresize-test.html'
    timeout = 150
    gecko_profile_interval = 2
    gecko_profile_entries = 1000000
    tpmozafterpaint = True
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


# pageloader tests(tp5, etc)

# The overall test number is determined by first calculating the median
# page load time for each page in the set (excluding the max page load
# per individual page). The max median from that set is then excluded and
# the average is taken; that becomes the number reported to the tinderbox
# waterfall.


class PageloaderTest(Test):
    """abstract base class for a Talos Pageloader test"""
    tpmanifest = None  # test manifest
    tpcycles = 1  # number of time to run each page
    cycles = None
    timeout = None
    keys = ['tpmanifest', 'tpcycles', 'tppagecycles', 'tprender', 'tpchrome',
            'tpmozafterpaint', 'tploadnocache', 'rss', 'mainthread',
            'resolution', 'cycles', 'gecko_profile', 'gecko_profile_interval',
            'gecko_profile_entries', 'tptimeout', 'win_counters', 'w7_counters',
            'linux_counters', 'mac_counters', 'tpscrolltest', 'xperf_counters',
            'timeout', 'shutdown', 'responsiveness', 'profile_path',
            'xperf_providers', 'xperf_user_providers', 'xperf_stackwalk',
            'filters', 'preferences', 'extensions', 'setup', 'cleanup',
            'lower_is_better', 'alert_threshold', 'unit', 'webextensions']


class QuantumPageloadTest(PageloaderTest):
    """
    Base class for a Quantum Pageload test
    """
    tpcycles = 1
    tppagecycles = 25
    gecko_profile_interval = 1
    gecko_profile_entries = 2000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'
    lower_is_better = True


@register_test()
class tabpaint(PageloaderTest):
    """
    Tests the amount of time it takes to open new tabs, triggered from
    both the parent process and the content process.
    """
    extensions = '${talos}/tests/tabpaint/tabpaint-signed.xpi'
    tpmanifest = '${talos}/tests/tabpaint/tabpaint.manifest'
    tppagecycles = 20
    gecko_profile_entries = 1000000
    tploadnocache = True
    unit = 'ms'
    preferences = {
        # By default, Talos is configured to open links from
        # content in new windows. We're overriding them so that
        # they open in new tabs instead.
        # See http://kb.mozillazine.org/Browser.link.open_newwindow
        # and http://kb.mozillazine.org/Browser.link.open_newwindow.restriction
        'browser.link.open_newwindow': 3,
        'browser.link.open_newwindow.restriction': 2,
    }


@register_test()
class tps(PageloaderTest):
    """
    Tests the amount of time it takes to switch between tabs
    """
    extensions = '${talos}/tests/tabswitch/tabswitch-signed.xpi'
    tpmanifest = '${talos}/tests/tabswitch/tps.manifest'
    tppagecycles = 5
    gecko_profile_entries = 1000000
    tploadnocache = True
    preferences = {
        'addon.test.tabswitch.urlfile': os.path.join('${talos}',
                                                     'tests',
                                                     'tp5o.html'),
        'addon.test.tabswitch.webserver': '${webserver}',
        # limit the page set number for winxp as we have issues.
        # see https://bugzilla.mozilla.org/show_bug.cgi?id=1195288
        'addon.test.tabswitch.maxurls':
            45 if utils.PLATFORM_TYPE == 'win_' else -1,
    }
    unit = 'ms'


@register_test()
class tart(PageloaderTest):
    """
    Tab Animation Regression Test
    Tests tab animation on these cases:
    1. Simple: single new tab of about:blank open/close without affecting
       (shrinking/expanding) other tabs.
    2. icon: same as above with favicons and long title instead of about:blank.
    3. Newtab: newtab open with thumbnails preview - without affecting other
       tabs, with and without preload.
    4. Fade: opens a tab, then measures fadeout/fadein (tab animation without
       the overhead of opening/closing a tab).
    - Case 1 is tested with DPI scaling of 1.
    - Case 2 is tested with DPI scaling of 1.0 and 2.0.
    - Case 3 is tested with the default scaling of the test system.
    - Case 4 is tested with DPI scaling of 2.0 with the "icon" tab
      (favicon and long title).
    - Each animation produces 3 test results:
      - error: difference between the designated duration and the actual
        completion duration from the trigger.
      - half: average interval over the 2nd half of the animation.
      - all: average interval over all recorded intervals.
    """
    tpmanifest = '${talos}/tests/tart/tart.manifest'
    extensions = '${talos}/tests/tart/addon/tart-signed.xpi'
    tpcycles = 1
    tppagecycles = 25
    tploadnocache = True
    tpmozafterpaint = False
    gecko_profile_interval = 10
    gecko_profile_entries = 1000000
    win_counters = w7_counters = linux_counters = mac_counters = None
    """
    ASAP mode
    The recording API is broken with OMTC before ~2013-11-27
    After ~2013-11-27, disabling OMTC will also implicitly disable
    OGL HW composition to disable OMTC with older firefox builds, also
    set 'layers.offmainthreadcomposition.enabled': False
    """
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'ms'


@register_test()
class cart(PageloaderTest):
    """
    Customize Animation Regression Test
    Tests Australis customize animations (default DPI scaling). Uses the
    TART addon but with a different URL.
    Reports the same animation values as TART (.half/.all/.error).
    All comments for TART also apply here (e.g. for ASAP+OMTC, etc)
    Subtests are:
    1-customize-enter - from triggering customize mode until it's ready for
    the user
    2-customize-exit  - exiting customize
    3-customize-enter-css - only the CSS animation part of entering customize
    """
    tpmanifest = '${talos}/tests/tart/cart.manifest'
    extensions = '${talos}/tests/tart/addon/tart-signed.xpi'
    tpcycles = 1
    tppagecycles = 25
    tploadnocache = True
    tpmozafterpaint = False
    gecko_profile_interval = 1
    gecko_profile_entries = 10000000
    win_counters = w7_counters = linux_counters = mac_counters = None
    """
    ASAP mode
    """
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'ms'


@register_test()
class damp(PageloaderTest):
    """
    Devtools At Maximum Performance
    Tests the speed of DevTools toolbox open, close, and page reload
    for each tool, across a very simple and very complicated page.
    """
    tpmanifest = '${talos}/tests/devtools/damp.manifest'
    extensions = '${talos}/tests/devtools/addon/devtools-signed.xpi'
    tpcycles = 1
    tppagecycles = 25
    tploadnocache = True
    tpmozafterpaint = False
    gecko_profile_interval = 10
    gecko_profile_entries = 1000000
    win_counters = w7_counters = linux_counters = mac_counters = None
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    preferences = {'devtools.memory.enabled': True,
                   'addon.test.damp.webserver': '${webserver}'}
    unit = 'ms'


@register_test()
class glterrain(PageloaderTest):
    """
    Simple rotating WebGL scene with moving light source over a
    textured terrain.
    Measures average frame intervals.
    The same sequence is measured 4 times for combinations of alpha and
    antialias as canvas properties.
    Each of these 4 runs is reported as a different test name.
    """
    tpmanifest = '${talos}/tests/webgl/glterrain.manifest'
    tpcycles = 1
    tppagecycles = 25
    tploadnocache = True
    tpmozafterpaint = False
    gecko_profile_interval = 10
    gecko_profile_entries = 2000000
    win_counters = w7_counters = linux_counters = mac_counters = None
    """ ASAP mode """
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'frame interval'


@register_test()
class glvideo(PageloaderTest):
    """
    WebGL video texture update with 1080p video.
    Measures mean tick time across 100 ticks.
    (each tick is texImage2D(<video>)+setTimeout(0))
    """
    tpmanifest = '${talos}/tests/webgl/glvideo.manifest'
    tpcycles = 1
    tppagecycles = 5
    tploadnocache = True
    tpmozafterpaint = False
    gecko_profile_interval = 2
    gecko_profile_entries = 2000000
    win_counters = w7_counters = linux_counters = mac_counters = None
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tp5n(PageloaderTest):
    """
    Tests the time it takes Firefox to load the tp5 web page test set.

    The tp5 is an updated web page test set to 100 pages from April 8th, 2011.
    Effort was made for the pages to no longer be splash screens/login
    pages/home pages but to be pages that better reflect the actual content
    of the site in question.
    """
    resolution = 20
    shutdown = True
    tpmanifest = '${talos}/tests/tp5n/tp5n.manifest'
    tpcycles = 1
    tppagecycles = 1
    cycles = 1
    tpmozafterpaint = True
    tptimeout = 5000
    rss = True
    mainthread = True
    w7_counters = []
    win_counters = []
    linux_counters = []
    mac_counters = []
    xperf_counters = ['main_startup_fileio', 'main_startup_netio',
                      'main_normal_fileio', 'main_normal_netio',
                      'nonmain_startup_fileio', 'nonmain_normal_fileio',
                      'nonmain_normal_netio', 'mainthread_readcount',
                      'mainthread_readbytes', 'mainthread_writecount',
                      'mainthread_writebytes']
    xperf_providers = ['PROC_THREAD', 'LOADER', 'HARD_FAULTS', 'FILENAME',
                       'FILE_IO', 'FILE_IO_INIT']
    xperf_user_providers = ['Mozilla Generic Provider',
                            'Microsoft-Windows-TCPIP']
    xperf_stackwalk = ['FileCreate', 'FileRead', 'FileWrite', 'FileFlush',
                       'FileClose']
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    timeout = 1800
    setup = '${talos}/xtalos/start_xperf.py -c ${talos}/bcontroller.json'
    cleanup = '${talos}/xtalos/parse_xperf.py -c ${talos}/bcontroller.json'
    preferences = {'extensions.enabledScopes': '',
                   'talos.logfile': 'browser_output.txt'}
    unit = 'ms'


@register_test()
class tp5o(PageloaderTest):
    """
    Derived from the tp5n pageset, this is the 49 most reliable webpages.
    """
    tpcycles = 1
    tppagecycles = 25
    cycles = 1
    tpmozafterpaint = True
    tptimeout = 5000
    rss = True
    mainthread = False
    tpmanifest = '${talos}/tests/tp5n/tp5o.manifest'
    win_counters = ['Main_RSS', 'Private Bytes', '% Processor Time']
    w7_counters = ['Main_RSS', 'Private Bytes', '% Processor Time',
                   'Modified Page List Bytes']
    linux_counters = ['Private Bytes', 'XRes', 'Main_RSS']
    mac_counters = ['Main_RSS']
    responsiveness = True
    gecko_profile_interval = 2
    gecko_profile_entries = 4000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    timeout = 1800
    unit = 'ms'


@register_test()
class tp5o_webext(tp5o):
    webextensions = '${talos}/webextensions/dummy/dummy-signed.xpi'
    preferences = {'xpinstall.signatures.required': False}


@register_test()
class tp5o_scroll(PageloaderTest):
    """
    Tests scroll (like tscrollx does, including ASAP) but on the tp5o pageset.
    """
    tpmanifest = '${talos}/tests/tp5n/tp5o.manifest'
    tpcycles = 1
    tppagecycles = 12
    gecko_profile_interval = 2
    gecko_profile_entries = 2000000

    tpscrolltest = True
    """ASAP mode"""
    tpmozafterpaint = False
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False,
                   'layout.css.scroll-behavior.spring-constant': "'10'",
                   'toolkit.framesRecording.bufferSize': 10000}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = '1/FPS'


@register_test()
class v8_7(PageloaderTest):
    """
    This is the V8 (version 7) javascript benchmark taken verbatim and
    slightly modified to fit into our pageloader extension and talos harness.

    The previous version of this test is V8 version 5 which was run on
    selective branches and operating systems.
    """
    tpmanifest = '${talos}/tests/v8_7/v8.manifest'
    gecko_profile_interval = 1
    gecko_profile_entries = 1000000
    tpcycles = 1
    resolution = 20
    tpmozafterpaint = False
    preferences = {'dom.send_after_paint_to_content': False}
    filters = filter.v8_subtest.prepare()
    unit = 'score'
    lower_is_better = False


@register_test()
class kraken(PageloaderTest):
    """
    This is the Kraken javascript benchmark taken verbatim and slightly
    modified to fit into our pageloader extension and talos harness.
    """
    tpmanifest = '${talos}/tests/kraken/kraken.manifest'
    tpcycles = 1
    tppagecycles = 1
    gecko_profile_interval = 0.1
    gecko_profile_entries = 1000000
    tpmozafterpaint = False
    preferences = {'dom.send_after_paint_to_content': False}
    filters = filter.mean.prepare()
    unit = 'score'


@register_test()
class basic_compositor_video(PageloaderTest):
    """
    Video test
    """
    tpmanifest = '${talos}/tests/video/video.manifest'
    tpcycles = 1
    tppagecycles = 12
    timeout = 10000
    gecko_profile_interval = 1
    gecko_profile_entries = 2000000
    preferences = {'full-screen-api.allow-trusted-requests-only': False,
                   'layers.acceleration.force-enabled': False,
                   'layers.acceleration.disabled': True,
                   'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'full-screen-api.warning.timeout': 500,
                   'media.ruin-av-sync.enabled': True}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'ms/frame'
    lower_is_better = True


@register_test()
class tcanvasmark(PageloaderTest):
    """
    CanvasMark benchmark v0.6
    """
    tpmanifest = '${talos}/tests/canvasmark/canvasmark.manifest'
    win_counters = w7_counters = linux_counters = mac_counters = None
    tpcycles = 5
    tppagecycles = 1
    timeout = 900
    gecko_profile_interval = 10
    gecko_profile_entries = 2500000
    tpmozafterpaint = False
    preferences = {'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(1) + filter.median.prepare()
    unit = 'score'
    lower_is_better = False


class dromaeo(PageloaderTest):
    """abstract base class for dramaeo tests"""
    filters = filter.dromaeo.prepare()
    lower_is_better = False
    alert_threshold = 5.0


@register_test()
class dromaeo_css(dromaeo):
    """
    Dromaeo suite of tests for JavaScript performance testing.
    See the Dromaeo wiki (https://wiki.mozilla.org/Dromaeo)
    for more information.

    Each page in the manifest is part of the dromaemo css benchmark.
    """
    gecko_profile_interval = 2
    gecko_profile_entries = 10000000
    tpmanifest = '${talos}/tests/dromaeo/css.manifest'
    unit = 'score'


@register_test()
class dromaeo_dom(dromaeo):
    """
    Dromaeo suite of tests for JavaScript performance testing.
    See the Dromaeo wiki (https://wiki.mozilla.org/Dromaeo)
    for more information.

    Each page in the manifest is part of the dromaemo dom benchmark.
    """
    gecko_profile_interval = 2
    gecko_profile_entries = 10000000
    tpmanifest = '${talos}/tests/dromaeo/dom.manifest'
    tpdisable_e10s = True
    unit = 'score'


@register_test()
class tsvgm(PageloaderTest):
    """
    An svg-only number that measures SVG rendering performance
    for dynamic content only.
    """
    tpmanifest = '${talos}/tests/svgx/svgm.manifest'
    tpcycles = 1
    tppagecycles = 7
    tpmozafterpaint = False
    gecko_profile_interval = 10
    gecko_profile_entries = 1000000
    """ASAP mode"""
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(2) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tsvgx(PageloaderTest):
    """
    An svg-only number that measures SVG rendering performance
    for dynamic content only.
    """
    tpmanifest = '${talos}/tests/svgx/svgx.manifest'
    tpcycles = 1
    tppagecycles = 25
    tpmozafterpaint = False
    gecko_profile_interval = 10
    gecko_profile_entries = 1000000
    """ASAP mode"""
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False}
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tsvg_static(PageloaderTest):
    """
    An svg-only number that measures SVG rendering performance
    for static content only.
    """
    tpmanifest = '${talos}/tests/svg_static/svg_static.manifest'
    tpcycles = 1
    tppagecycles = 25
    tpmozafterpaint = True
    gecko_profile_interval = 1
    gecko_profile_entries = 10000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tsvgr_opacity(PageloaderTest):
    """
    An svg-only number that measures SVG rendering performance.
    """
    tpmanifest = '${talos}/tests/svg_opacity/svg_opacity.manifest'
    tpcycles = 1
    tppagecycles = 25
    tpmozafterpaint = True
    gecko_profile_interval = 1
    gecko_profile_entries = 10000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


@register_test()
class tscrollx(PageloaderTest):
    """
    This test does some scrolly thing.
    """
    tpmanifest = '${talos}/tests/scroll/scroll.manifest'
    tpcycles = 1
    tppagecycles = 25
    tpmozafterpaint = False
    gecko_profile_interval = 1
    gecko_profile_entries = 1000000
    """ ASAP mode """
    preferences = {'layout.frame_rate': 0,
                   'docshell.event_starvation_delay_hint': 1,
                   'dom.send_after_paint_to_content': False,
                   'layout.css.scroll-behavior.spring-constant': "'10'",
                   'toolkit.framesRecording.bufferSize': 10000}
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'


@register_test()
class a11yr(PageloaderTest):
    """
    This test ensures basic a11y tables and permutations do not cause
    performance regressions.
    """
    tpmanifest = '${talos}/tests/a11y/a11y.manifest'
    tpcycles = 1
    tppagecycles = 25
    tpmozafterpaint = True
    preferences = {'dom.send_after_paint_to_content': False}
    unit = 'ms'
    alert_threshold = 5.0


@register_test()
class bloom_basic(PageloaderTest):
    """
    Stylo bloom_basic test
    """
    tpmanifest = '${talos}/tests/perf-reftest/bloom_basic.manifest'
    tpcycles = 1
    tppagecycles = 25
    gecko_profile_interval = 1
    gecko_profile_entries = 2000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'
    lower_is_better = True
    alert_threshold = 5.0


@register_test()
class bloom_basic_ref(PageloaderTest):
    """
    Stylo bloom_basic_ref test
    """
    tpmanifest = '${talos}/tests/perf-reftest/bloom_basic_ref.manifest'
    tpcycles = 1
    tppagecycles = 25
    gecko_profile_interval = 1
    gecko_profile_entries = 2000000
    filters = filter.ignore_first.prepare(5) + filter.median.prepare()
    unit = 'ms'
    lower_is_better = True
    alert_threshold = 5.0


@register_test()
class quantum_pageload_google(QuantumPageloadTest):
    """
    Quantum Pageload Test - Google
    """
    tpmanifest = '${talos}/tests/quantum_pageload/quantum_pageload_google.manifest'


@register_test()
class quantum_pageload_youtube(QuantumPageloadTest):
    """
    Quantum Pageload Test - YouTube
    """
    tpmanifest = '${talos}/tests/quantum_pageload/quantum_pageload_youtube.manifest'


@register_test()
class quantum_pageload_amazon(QuantumPageloadTest):
    """
    Quantum Pageload Test - Amazon
    """
    tpmanifest = '${talos}/tests/quantum_pageload/quantum_pageload_amazon.manifest'


@register_test()
class quantum_pageload_facebook(QuantumPageloadTest):
    """
    Quantum Pageload Test - Facebook
    """
    tpmanifest = '${talos}/tests/quantum_pageload/quantum_pageload_facebook.manifest'
