######
Raptor
######

Raptor is a performance-testing framework for running browser pageload and browser benchmark tests. Raptor is cross-browser compatible and is currently running in production on Firefox Desktop, Firefox Android GeckoView, Fenix, Reference Browser, Chromium, and Chrome.

- Contact: Dave Hunt [:davehunt]
- Source code: https://searchfox.org/mozilla-central/source/testing/raptor
- Good first bugs: https://codetribute.mozilla.org/projects/automation?project%3DRaptor

Raptor currently supports three test types: 1) page-load performance tests, 2) standard benchmark-performance tests, and 3) "scenario"-based tests, such as power, CPU, and memory-usage measurements on Android (and desktop?).

Locally, Raptor can be invoked with the following command:

::

    ./mach raptor

**We're in the process of migrating away from webextension to browsertime. Currently, raptor supports both of them, but at the end of the migration, the support for webextension will be removed.**

.. toctree::
   :titlesonly:

   browsertime
   webextension
   debugging
   contributing
   test-list

.. contents::
   :depth: 2
   :local:

Raptor tests
************

The following documents all testing we have for Raptor.

Benchmarks
----------
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI. (FF: Firefox, CH: Chrome, CU: Chromium)

.. dropdown:: ares6 (FF, CH, CU)
   :container: + anchor-id-ares6-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 4
   * **page timeout**: 270000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/ARES-6/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-ares6-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: trunk
            * browsertime-benchmark-firefox-ares6-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-ares6-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-ares6-fis-e10s: mozilla-central


.. dropdown:: assorted-dom (FF, CH, CU)
   :container: + anchor-id-assorted-dom-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **fetch task**: assorted-dom
   * **gecko profile entries**: 2000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 60000
   * **screen capture**: true
   * **test url**: `<http://\<host\>:\<port\>/assorted-dom/assorted/driver.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: trunk
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: mozilla-central


.. dropdown:: jetstream2 (FF, CH, CU)
   :container: + anchor-id-jetstream2-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **fetch task**: jetstream2
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 4
   * **page timeout**: 2000000
   * **subtest lower is better**: false
   * **subtest unit**: score
   * **test url**: `<http://\<host\>:\<port\>/JetStream2/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: trunk
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central


.. dropdown:: motionmark-animometer (FF, CH, CU)
   :container: + anchor-id-motionmark-animometer-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&raptor=true&oskey={platform}>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: trunk
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: mozilla-central


.. dropdown:: motionmark-htmlsuite (FF, CH, CU)
   :container: + anchor-id-motionmark-htmlsuite-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=HTMLsuite&raptor=true&oskey={platform}>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: trunk
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: mozilla-central


.. dropdown:: raptor-speedometer-geckoview (GV)
   :container: + anchor-id-raptor-speedometer-geckoview-b

   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 420000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/Speedometer/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score


.. dropdown:: raptor-youtube-playback-h264-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-1080p60-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-full-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-full-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-full-1080p60-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-full-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-v9-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-v9-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-v9-1080p60-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-v9-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-v9-full-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-v9-full-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-v9-full-1080p60-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-v9-full-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: speedometer (FF, CH, CU, FE, GV, RB, CH-M)
   :container: + anchor-id-speedometer-b

   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 180000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/Speedometer/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central

   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow, chrome-m
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 420000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/Speedometer/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central


.. dropdown:: stylebench (FF, CH, CU)
   :container: + anchor-id-stylebench-b

   **Owner**: :emelio and Layout Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 120000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/StyleBench/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-stylebench-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: trunk
            * browsertime-benchmark-firefox-stylebench-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-stylebench-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-stylebench-fis-e10s: mozilla-central


.. dropdown:: sunspider (FF, CH, CU)
   :container: + anchor-id-sunspider-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 55000
   * **test url**: `<http://\<host\>:\<port\>/SunSpider/sunspider-1.0.1/sunspider-1.0.1/driver.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-sunspider-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: trunk
            * browsertime-benchmark-firefox-sunspider-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-sunspider-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-sunspider-fis-e10s: mozilla-central


.. dropdown:: unity-webgl (FF, CH, CU, FE, RB, FE, CH-M)
   :container: + anchor-id-unity-webgl-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: geckoview, refbrow, fenix, chrome-m
   * **expected**: pass
   * **fetch task**: unity-webgl
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 420000
   * **test url**: `<http://\<host\>:\<port\>/unity-webgl/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **fetch task**: unity-webgl
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 420000
   * **test url**: `<http://\<host\>:\<port\>/unity-webgl/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central


.. dropdown:: wasm-godot (FF, CH, CU)
   :container: + anchor-id-wasm-godot-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **newtab per cycle**: true
   * **page cycles**: 5
   * **page timeout**: 120000
   * **test url**: `<http://localhost:\<port\>/wasm-godot/index.html>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: mozilla-central


.. dropdown:: wasm-godot-baseline (FF)
   :container: + anchor-id-wasm-godot-baseline-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **newtab per cycle**: true
   * **page cycles**: 5
   * **page timeout**: 120000
   * **preferences**: {"javascript.options.wasm_baselinejit": true, "javascript.options.wasm_optimizingjit": false}
   * **test url**: `<http://localhost:\<port\>/wasm-godot/index.html>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: mozilla-central


.. dropdown:: wasm-godot-optimizing (FF)
   :container: + anchor-id-wasm-godot-optimizing-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **newtab per cycle**: true
   * **page cycles**: 5
   * **page timeout**: 120000
   * **preferences**: {"javascript.options.wasm_baselinejit": false, "javascript.options.wasm_optimizingjit": true}
   * **test url**: `<http://localhost:\<port\>/wasm-godot/index.html>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: mozilla-central


.. dropdown:: wasm-misc (FF, CH, CU)
   :container: + anchor-id-wasm-misc-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **fetch task**: wasm-misc
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **test url**: `<http://\<host\>:\<port\>/wasm-misc/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: mozilla-central


.. dropdown:: wasm-misc-baseline (FF)
   :container: + anchor-id-wasm-misc-baseline-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **fetch task**: wasm-misc
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **preferences**: {"javascript.options.wasm_baselinejit": true, "javascript.options.wasm_optimizingjit": false}
   * **test url**: `<http://\<host\>:\<port\>/wasm-misc/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: mozilla-central


.. dropdown:: wasm-misc-optimizing (FF)
   :container: + anchor-id-wasm-misc-optimizing-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **fetch task**: wasm-misc
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **preferences**: {"javascript.options.wasm_baselinejit": false, "javascript.options.wasm_optimizingjit": true}
   * **test url**: `<http://\<host\>:\<port\>/wasm-misc/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: mozilla-central


.. dropdown:: webaudio (FF, CH, CU)
   :container: + anchor-id-webaudio-b

   **Owner**: :padenot and Media Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 360000
   * **test url**: `<http://\<host\>:\<port\>/webaudio/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-webaudio-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: trunk
            * browsertime-benchmark-firefox-webaudio-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-webaudio-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-webaudio-fis-e10s: mozilla-central


.. dropdown:: youtube-playback (FF, GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-b

   **Owner**: PerfTest Team

   * **alert on**: H264.1080p30@1X_dropped_frames, H264.1080p60@1X_dropped_frames, H264.1440p30@1X_dropped_frames, H264.144p15@1X_dropped_frames, H264.2160p30@1X_dropped_frames, H264.240p30@1X_dropped_frames, H264.360p30@1X_dropped_frames, H264.480p30@1X_dropped_frames, H264.720p30@1X_dropped_frames, H264.720p60@1X_dropped_frames, VP9.1080p30@1X_dropped_frames, VP9.1080p60@1X_dropped_frames, VP9.1440p30@1X_dropped_frames, VP9.1440p60@1X_dropped_frames, VP9.144p30@1X_dropped_frames, VP9.2160p30@1X_dropped_frames, VP9.2160p60@1X_dropped_frames, VP9.240p30@1X_dropped_frames, VP9.360p30@1X_dropped_frames, VP9.480p30@1X_dropped_frames, VP9.720p30@1X_dropped_frames, VP9.720p60@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix,refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<http://yttest.prod.mozaws.net/2019/main.html?test_type=playbackperf-test&raptor=true&command=run&exclude=1,2&muted=true>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-av1-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-av1-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-av1-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-av1-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-h264-1080p30 (FF)
   :container: + anchor-id-youtube-playback-h264-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-h264-1080p60 (FF)
   :container: + anchor-id-youtube-playback-h264-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-h264-full-1080p30 (FF)
   :container: + anchor-id-youtube-playback-h264-full-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-h264-full-1080p60 (FF)
   :container: + anchor-id-youtube-playback-h264-full-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-h264-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-h264-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-hfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-hfr-b

   **Owner**: PerfTest Team

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-v9-1080p30 (FF)
   :container: + anchor-id-youtube-playback-v9-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-v9-1080p60 (FF)
   :container: + anchor-id-youtube-playback-v9-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-v9-full-1080p30 (FF)
   :container: + anchor-id-youtube-playback-v9-full-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-v9-full-1080p60 (FF)
   :container: + anchor-id-youtube-playback-v9-full-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: youtube-playback-vp9-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-vp9-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-h264-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-h264-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-hfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-hfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-vp9-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-vp9-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central



Custom
------
Browsertime tests that use a custom pageload test script. These use the pageload type, but may have other intentions.

.. dropdown:: process-switch (Measures process switch time)
   :container: + anchor-id-process-switch-c

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **browsertime args**: --pageCompleteWaitTime=1000 --pageCompleteCheckInactivity=true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-proc-switch.manifest
   * **playback version**: 5.1.1
   * **test script**: process_switch.js
   * **test url**: `<https://mozilla.seanfeng.dev/files/red.html,https://mozilla.pettay.fi/moztests/blue.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: mozilla-central
            * browsertime-custom-firefox-process-switch-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: mozilla-central
            * browsertime-custom-firefox-process-switch-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: mozilla-central
            * browsertime-custom-firefox-process-switch-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: mozilla-central
            * browsertime-custom-firefox-process-switch-fis-e10s: mozilla-central


.. dropdown:: welcome (Measures pageload metrics for the first-install about:welcome page)
   :container: + anchor-id-welcome-c

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-welcome.manifest
   * **playback version**: 5.1.1
   * **test script**: welcome.js
   * **test url**: `<about:welcome>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: trunk
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: trunk
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: trunk
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: trunk
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central



Desktop
-------
Tests for page-load performance. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, FF: Firefox, CH: Chrome, CU: Chromium)

.. dropdown:: amazon (BT, FF, CH, CU)
   :container: + anchor-id-amazon-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **playback version**: 7.0.4
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
            * browsertime-tp6-profiling-firefox-amazon-e10s: mozilla-central
            * browsertime-tp6-profiling-firefox-amazon-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: trunk
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
            * browsertime-tp6-profiling-firefox-amazon-e10s: mozilla-central
            * browsertime-tp6-profiling-firefox-amazon-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: None
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None


.. dropdown:: bing-search (BT, FF, CH, CU)
   :container: + anchor-id-bing-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: trunk
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: None
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None


.. dropdown:: buzzfeed (BT, FF, CH, CU)
   :container: + anchor-id-buzzfeed-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-buzzfeed.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.buzzfeed.com/quizzes>`__
   * **test url**: `<https://www.buzzfeed.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: trunk
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: mozilla-central


.. dropdown:: cnn (BT, FF, CH, CU)
   :container: + anchor-id-cnn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-cnn.manifest
   * **playback version**: 6.0.2
   * **preferences**: {"media.autoplay.default": 5, "media.autoplay.ask-permission": true, "media.autoplay.blocking_policy": 1, "media.autoplay.block-webaudio": true, "media.allowed-to-play.enabled": false, "media.block-autoplay-until-in-foreground": true}
   * **secondary url**: `<https://www.cnn.com/weather>`__
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None


.. dropdown:: ebay (BT, FF, CH, CU)
   :container: + anchor-id-ebay-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-darwin-firefox-ebay.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.ebay.com/deals>`__
   * **test url**: `<https://www.ebay.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-fis-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-fis-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-ebay-e10s: None
            * browsertime-tp6-chromium-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-ebay-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-ebay-e10s: None
            * browsertime-tp6-live-chromium-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-ebay-e10s: None
            * browsertime-tp6-chromium-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-e10s: trunk
            * browsertime-tp6-firefox-ebay-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-ebay-e10s: None
            * browsertime-tp6-live-chromium-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-ebay-e10s: None
            * browsertime-tp6-chromium-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-ebay-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-ebay-e10s: None
            * browsertime-tp6-live-chromium-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-fis-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-ebay-e10s: None
            * browsertime-tp6-chromium-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-fis-e10s: None
            * browsertime-tp6-live-chrome-ebay-e10s: None
            * browsertime-tp6-live-chromium-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-fis-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-ebay-e10s: None
            * browsertime-tp6-chromium-ebay-e10s: None
            * browsertime-tp6-firefox-ebay-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-ebay-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-ebay-e10s: None
            * browsertime-tp6-live-chromium-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-fis-e10s: None


.. dropdown:: espn (BT, FF, CH, CU)
   :container: + anchor-id-espn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-espn.manifest
   * **playback version**: 5.1.1
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-espn-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: trunk
            * browsertime-tp6-firefox-espn-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-espn-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-espn-fis-e10s: mozilla-central


.. dropdown:: expedia (BT, FF, CH, CU)
   :container: + anchor-id-expedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-expedia.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.expedia.com/Activities>`__
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central


.. dropdown:: facebook (BT, FF, CH, CU)
   :container: + anchor-id-facebook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-facebook.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.facebook.com/marketplace/?ref=bookmark>`__
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-fis-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-fis-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-facebook-e10s: None
            * browsertime-tp6-chromium-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-facebook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-facebook-e10s: None
            * browsertime-tp6-live-chromium-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-facebook-e10s: None
            * browsertime-tp6-chromium-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-e10s: trunk
            * browsertime-tp6-firefox-facebook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-facebook-e10s: None
            * browsertime-tp6-live-chromium-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-facebook-e10s: None
            * browsertime-tp6-chromium-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-facebook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-facebook-e10s: None
            * browsertime-tp6-live-chromium-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-fis-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-facebook-e10s: None
            * browsertime-tp6-chromium-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-fis-e10s: None
            * browsertime-tp6-live-chrome-facebook-e10s: None
            * browsertime-tp6-live-chromium-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-fis-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-facebook-e10s: None
            * browsertime-tp6-chromium-facebook-e10s: None
            * browsertime-tp6-firefox-facebook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-facebook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-facebook-e10s: None
            * browsertime-tp6-live-chromium-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-fis-e10s: None


.. dropdown:: fandom (BT, FF, CH, CU)
   :container: + anchor-id-fandom-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: trunk
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: None
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None


.. dropdown:: google-docs (BT, FF, CH, CU)
   :container: + anchor-id-google-docs-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page complete wait time**: 8000
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-google-docs.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://docs.google.com/document/d/1vUnn0ePU-ynArE1OdxyEHXR2G0sl74ja_st_4OOzlgE/preview>`__
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-fis-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-fis-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-e10s: None
            * browsertime-tp6-chromium-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-docs-e10s: None
            * browsertime-tp6-live-chromium-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-e10s: None
            * browsertime-tp6-chromium-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-e10s: trunk
            * browsertime-tp6-firefox-google-docs-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-docs-e10s: None
            * browsertime-tp6-live-chromium-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-e10s: None
            * browsertime-tp6-chromium-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-docs-e10s: None
            * browsertime-tp6-live-chromium-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-fis-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-e10s: None
            * browsertime-tp6-chromium-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-fis-e10s: None
            * browsertime-tp6-live-chrome-google-docs-e10s: None
            * browsertime-tp6-live-chromium-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-fis-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-e10s: None
            * browsertime-tp6-chromium-google-docs-e10s: None
            * browsertime-tp6-firefox-google-docs-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-docs-e10s: None
            * browsertime-tp6-live-chromium-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-fis-e10s: None


.. dropdown:: google-docs-canvas (BT, FF, CH, CU)
   :container: + anchor-id-google-docs-canvas-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-google-docs-canvas.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://docs.google.com/document/d/1vUnn0ePU-ynArE1OdxyEHXR2G0sl74ja_st_4OOzlgE/preview>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: trunk
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: mozilla-central


.. dropdown:: google-mail (BT, FF, CH, CU)
   :container: + anchor-id-google-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-mail-e10s: None
            * browsertime-tp6-chromium-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-mail-e10s: None
            * browsertime-tp6-live-chromium-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-mail-e10s: None
            * browsertime-tp6-chromium-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-e10s: trunk
            * browsertime-tp6-firefox-google-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-mail-e10s: None
            * browsertime-tp6-live-chromium-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-mail-e10s: None
            * browsertime-tp6-chromium-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-mail-e10s: None
            * browsertime-tp6-live-chromium-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-google-mail-e10s: None
            * browsertime-tp6-chromium-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-fis-e10s: None
            * browsertime-tp6-live-chrome-google-mail-e10s: None
            * browsertime-tp6-live-chromium-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-mail-e10s: None
            * browsertime-tp6-chromium-google-mail-e10s: None
            * browsertime-tp6-firefox-google-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-mail-e10s: None
            * browsertime-tp6-live-chromium-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-fis-e10s: None


.. dropdown:: google-search (BT, FF, CH, CU)
   :container: + anchor-id-google-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-fis-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-fis-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-search-e10s: None
            * browsertime-tp6-chromium-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-search-e10s: None
            * browsertime-tp6-live-chromium-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-search-e10s: None
            * browsertime-tp6-chromium-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-e10s: trunk
            * browsertime-tp6-firefox-google-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-search-e10s: None
            * browsertime-tp6-live-chromium-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-search-e10s: None
            * browsertime-tp6-chromium-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-search-e10s: None
            * browsertime-tp6-live-chromium-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-fis-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-google-search-e10s: None
            * browsertime-tp6-chromium-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-fis-e10s: None
            * browsertime-tp6-live-chrome-google-search-e10s: None
            * browsertime-tp6-live-chromium-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-fis-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-search-e10s: None
            * browsertime-tp6-chromium-google-search-e10s: None
            * browsertime-tp6-firefox-google-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-search-e10s: None
            * browsertime-tp6-live-chromium-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-fis-e10s: None


.. dropdown:: google-slides (BT, FF, CH, CU)
   :container: + anchor-id-google-slides-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-google-slides.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://docs.google.com/document/d/1vUnn0ePU-ynArE1OdxyEHXR2G0sl74ja_st_4OOzlgE/preview>`__
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: trunk
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: None
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None


.. dropdown:: imdb (BT, FF, CH, CU)
   :container: + anchor-id-imdb-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-imdb.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.imdb.com/title/tt0084967/episodes/?ref_=tt_ov_epl>`__
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-fis-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-fis-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-imdb-e10s: None
            * browsertime-tp6-chromium-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imdb-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imdb-e10s: None
            * browsertime-tp6-live-chromium-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-imdb-e10s: None
            * browsertime-tp6-chromium-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-e10s: trunk
            * browsertime-tp6-firefox-imdb-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imdb-e10s: None
            * browsertime-tp6-live-chromium-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-imdb-e10s: None
            * browsertime-tp6-chromium-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imdb-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imdb-e10s: None
            * browsertime-tp6-live-chromium-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-fis-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-imdb-e10s: None
            * browsertime-tp6-chromium-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-fis-e10s: None
            * browsertime-tp6-live-chrome-imdb-e10s: None
            * browsertime-tp6-live-chromium-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-fis-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-imdb-e10s: None
            * browsertime-tp6-chromium-imdb-e10s: None
            * browsertime-tp6-firefox-imdb-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imdb-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imdb-e10s: None
            * browsertime-tp6-live-chromium-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-fis-e10s: None


.. dropdown:: imgur (BT, FF, CH, CU)
   :container: + anchor-id-imgur-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-imgur.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-fis-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-fis-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-imgur-e10s: None
            * browsertime-tp6-chromium-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imgur-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imgur-e10s: None
            * browsertime-tp6-live-chromium-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-imgur-e10s: None
            * browsertime-tp6-chromium-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-e10s: trunk
            * browsertime-tp6-firefox-imgur-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imgur-e10s: None
            * browsertime-tp6-live-chromium-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-imgur-e10s: None
            * browsertime-tp6-chromium-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imgur-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imgur-e10s: None
            * browsertime-tp6-live-chromium-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-fis-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-imgur-e10s: None
            * browsertime-tp6-chromium-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-fis-e10s: None
            * browsertime-tp6-live-chrome-imgur-e10s: None
            * browsertime-tp6-live-chromium-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-fis-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-imgur-e10s: None
            * browsertime-tp6-chromium-imgur-e10s: None
            * browsertime-tp6-firefox-imgur-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-imgur-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-imgur-e10s: None
            * browsertime-tp6-live-chromium-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-fis-e10s: None


.. dropdown:: instagram (BT, FF, CH, CU)
   :container: + anchor-id-instagram-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-instagram.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.instagram.com/nobelprize_org/>`__
   * **test url**: `<https://www.instagram.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: trunk
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: None
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None


.. dropdown:: linkedin (BT, FF, CH, CU)
   :container: + anchor-id-linkedin-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-linkedin.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__
   * **test url**: `<https://www.linkedin.com/feed/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-fis-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-fis-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-linkedin-e10s: None
            * browsertime-tp6-chromium-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-linkedin-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-linkedin-e10s: None
            * browsertime-tp6-live-chromium-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-linkedin-e10s: None
            * browsertime-tp6-chromium-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-e10s: trunk
            * browsertime-tp6-firefox-linkedin-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-linkedin-e10s: None
            * browsertime-tp6-live-chromium-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-linkedin-e10s: None
            * browsertime-tp6-chromium-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-linkedin-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-linkedin-e10s: None
            * browsertime-tp6-live-chromium-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-fis-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-linkedin-e10s: None
            * browsertime-tp6-chromium-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-fis-e10s: None
            * browsertime-tp6-live-chrome-linkedin-e10s: None
            * browsertime-tp6-live-chromium-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-fis-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-linkedin-e10s: None
            * browsertime-tp6-chromium-linkedin-e10s: None
            * browsertime-tp6-firefox-linkedin-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-linkedin-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-linkedin-e10s: None
            * browsertime-tp6-live-chromium-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-fis-e10s: None


.. dropdown:: microsoft (BT, FF, CH, CU)
   :container: + anchor-id-microsoft-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-microsoft.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://support.microsoft.com/en-us>`__
   * **test url**: `<https://www.microsoft.com/en-us/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-fis-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-fis-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-microsoft-e10s: None
            * browsertime-tp6-chromium-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-microsoft-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-microsoft-e10s: None
            * browsertime-tp6-live-chromium-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-microsoft-e10s: None
            * browsertime-tp6-chromium-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-e10s: trunk
            * browsertime-tp6-firefox-microsoft-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-microsoft-e10s: None
            * browsertime-tp6-live-chromium-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-microsoft-e10s: None
            * browsertime-tp6-chromium-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-microsoft-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-microsoft-e10s: None
            * browsertime-tp6-live-chromium-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-fis-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-microsoft-e10s: None
            * browsertime-tp6-chromium-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-fis-e10s: None
            * browsertime-tp6-live-chrome-microsoft-e10s: None
            * browsertime-tp6-live-chromium-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-fis-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-microsoft-e10s: None
            * browsertime-tp6-chromium-microsoft-e10s: None
            * browsertime-tp6-firefox-microsoft-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-microsoft-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-microsoft-e10s: None
            * browsertime-tp6-live-chromium-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-fis-e10s: None


.. dropdown:: netflix (BT, FF, CH, CU)
   :container: + anchor-id-netflix-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-netflix.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.netflix.com/title/699257>`__
   * **test url**: `<https://www.netflix.com/title/80117263>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-fis-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-fis-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-netflix-e10s: None
            * browsertime-tp6-chromium-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-netflix-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-netflix-e10s: None
            * browsertime-tp6-live-chromium-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-netflix-e10s: None
            * browsertime-tp6-chromium-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-e10s: trunk
            * browsertime-tp6-firefox-netflix-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-netflix-e10s: None
            * browsertime-tp6-live-chromium-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-netflix-e10s: None
            * browsertime-tp6-chromium-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-netflix-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-netflix-e10s: None
            * browsertime-tp6-live-chromium-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-fis-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-netflix-e10s: None
            * browsertime-tp6-chromium-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-fis-e10s: None
            * browsertime-tp6-live-chrome-netflix-e10s: None
            * browsertime-tp6-live-chromium-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-fis-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-netflix-e10s: None
            * browsertime-tp6-chromium-netflix-e10s: None
            * browsertime-tp6-firefox-netflix-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-netflix-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-netflix-e10s: None
            * browsertime-tp6-live-chromium-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-fis-e10s: None


.. dropdown:: nytimes (BT, FF, CH, CU)
   :container: + anchor-id-nytimes-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-nytimes.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.nytimes.com/section/opinion/columnists>`__
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central


.. dropdown:: office (BT, FF, CH, CU)
   :container: + anchor-id-office-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-live-office.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.office.com/>`__
   * **test url**: `<https://www.office.com/launch/word>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-office-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: trunk
            * browsertime-tp6-firefox-office-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-office-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-office-fis-e10s: mozilla-central


.. dropdown:: outlook (BT, FF, CH, CU)
   :container: + anchor-id-outlook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://outlook.live.com/mail/inbox>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-fis-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-fis-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-outlook-e10s: None
            * browsertime-tp6-chromium-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-outlook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-outlook-e10s: None
            * browsertime-tp6-live-chromium-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-outlook-e10s: None
            * browsertime-tp6-chromium-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-e10s: trunk
            * browsertime-tp6-firefox-outlook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-outlook-e10s: None
            * browsertime-tp6-live-chromium-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-outlook-e10s: None
            * browsertime-tp6-chromium-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-outlook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-outlook-e10s: None
            * browsertime-tp6-live-chromium-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-fis-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-outlook-e10s: None
            * browsertime-tp6-chromium-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-fis-e10s: None
            * browsertime-tp6-live-chrome-outlook-e10s: None
            * browsertime-tp6-live-chromium-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-fis-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-outlook-e10s: None
            * browsertime-tp6-chromium-outlook-e10s: None
            * browsertime-tp6-firefox-outlook-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-outlook-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-outlook-e10s: None
            * browsertime-tp6-live-chromium-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-fis-e10s: None


.. dropdown:: paypal (BT, FF, CH, CU)
   :container: + anchor-id-paypal-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-fis-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-fis-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-paypal-e10s: None
            * browsertime-tp6-chromium-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-paypal-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-paypal-e10s: None
            * browsertime-tp6-live-chromium-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-paypal-e10s: None
            * browsertime-tp6-chromium-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-e10s: trunk
            * browsertime-tp6-firefox-paypal-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-paypal-e10s: None
            * browsertime-tp6-live-chromium-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-paypal-e10s: None
            * browsertime-tp6-chromium-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-paypal-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-paypal-e10s: None
            * browsertime-tp6-live-chromium-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-fis-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-paypal-e10s: None
            * browsertime-tp6-chromium-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-fis-e10s: None
            * browsertime-tp6-live-chrome-paypal-e10s: None
            * browsertime-tp6-live-chromium-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-fis-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-paypal-e10s: None
            * browsertime-tp6-chromium-paypal-e10s: None
            * browsertime-tp6-firefox-paypal-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-paypal-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-paypal-e10s: None
            * browsertime-tp6-live-chromium-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-fis-e10s: None


.. dropdown:: pinterest (BT, FF, CH, CU)
   :container: + anchor-id-pinterest-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-pinterest.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.pinterest.com/today/best/halloween-costumes-for-your-furry-friends/75787/>`__
   * **test url**: `<https://pinterest.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-fis-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-fis-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-pinterest-e10s: None
            * browsertime-tp6-chromium-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-pinterest-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-pinterest-e10s: None
            * browsertime-tp6-live-chromium-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-pinterest-e10s: None
            * browsertime-tp6-chromium-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-e10s: trunk
            * browsertime-tp6-firefox-pinterest-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-pinterest-e10s: None
            * browsertime-tp6-live-chromium-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-pinterest-e10s: None
            * browsertime-tp6-chromium-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-pinterest-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-pinterest-e10s: None
            * browsertime-tp6-live-chromium-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-fis-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-pinterest-e10s: None
            * browsertime-tp6-chromium-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-fis-e10s: None
            * browsertime-tp6-live-chrome-pinterest-e10s: None
            * browsertime-tp6-live-chromium-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-fis-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-pinterest-e10s: None
            * browsertime-tp6-chromium-pinterest-e10s: None
            * browsertime-tp6-firefox-pinterest-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-pinterest-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-pinterest-e10s: None
            * browsertime-tp6-live-chromium-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-fis-e10s: None


.. dropdown:: reddit (BT, FF, CH, CU)
   :container: + anchor-id-reddit-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-reddit.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.reddit.com/r/technology/>`__
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-fis-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-fis-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-reddit-e10s: None
            * browsertime-tp6-chromium-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-reddit-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-reddit-e10s: None
            * browsertime-tp6-live-chromium-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-reddit-e10s: None
            * browsertime-tp6-chromium-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-e10s: trunk
            * browsertime-tp6-firefox-reddit-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-reddit-e10s: None
            * browsertime-tp6-live-chromium-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-reddit-e10s: None
            * browsertime-tp6-chromium-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-reddit-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-reddit-e10s: None
            * browsertime-tp6-live-chromium-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-fis-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-reddit-e10s: None
            * browsertime-tp6-chromium-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-fis-e10s: None
            * browsertime-tp6-live-chrome-reddit-e10s: None
            * browsertime-tp6-live-chromium-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-fis-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-reddit-e10s: None
            * browsertime-tp6-chromium-reddit-e10s: None
            * browsertime-tp6-firefox-reddit-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-reddit-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-reddit-e10s: None
            * browsertime-tp6-live-chromium-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-fis-e10s: None


.. dropdown:: tumblr (BT, FF, CH, CU)
   :container: + anchor-id-tumblr-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-tumblr.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.tumblr.com/tagged/funny+cats?sort=top>`__
   * **test url**: `<https://www.tumblr.com/dashboard>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-fis-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-fis-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-tumblr-e10s: None
            * browsertime-tp6-chromium-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-tumblr-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-tumblr-e10s: None
            * browsertime-tp6-live-chromium-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-tumblr-e10s: None
            * browsertime-tp6-chromium-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-e10s: trunk
            * browsertime-tp6-firefox-tumblr-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-tumblr-e10s: None
            * browsertime-tp6-live-chromium-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-tumblr-e10s: None
            * browsertime-tp6-chromium-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-tumblr-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-tumblr-e10s: None
            * browsertime-tp6-live-chromium-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-fis-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-tumblr-e10s: None
            * browsertime-tp6-chromium-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-fis-e10s: None
            * browsertime-tp6-live-chrome-tumblr-e10s: None
            * browsertime-tp6-live-chromium-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-fis-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-tumblr-e10s: None
            * browsertime-tp6-chromium-tumblr-e10s: None
            * browsertime-tp6-firefox-tumblr-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-tumblr-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-tumblr-e10s: None
            * browsertime-tp6-live-chromium-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-fis-e10s: None


.. dropdown:: twitch (BT, FF, CH, CU)
   :container: + anchor-id-twitch-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-twitch.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-fis-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-fis-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-twitch-e10s: None
            * browsertime-tp6-chromium-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-twitch-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitch-e10s: None
            * browsertime-tp6-live-chromium-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-twitch-e10s: None
            * browsertime-tp6-chromium-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-e10s: trunk
            * browsertime-tp6-firefox-twitch-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitch-e10s: None
            * browsertime-tp6-live-chromium-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-twitch-e10s: None
            * browsertime-tp6-chromium-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-twitch-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitch-e10s: None
            * browsertime-tp6-live-chromium-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-fis-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-twitch-e10s: None
            * browsertime-tp6-chromium-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-fis-e10s: None
            * browsertime-tp6-live-chrome-twitch-e10s: None
            * browsertime-tp6-live-chromium-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-fis-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-twitch-e10s: None
            * browsertime-tp6-chromium-twitch-e10s: None
            * browsertime-tp6-firefox-twitch-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-twitch-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitch-e10s: None
            * browsertime-tp6-live-chromium-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-fis-e10s: None


.. dropdown:: twitter (BT, FF, CH, CU)
   :container: + anchor-id-twitter-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: trunk
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: None
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None


.. dropdown:: wikia (BT, FF, CH, CU)
   :container: + anchor-id-wikia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-wikia.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://marvel.fandom.com/wiki/Black_Panther>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-wikia-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: trunk
            * browsertime-tp6-firefox-wikia-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-wikia-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-wikia-fis-e10s: mozilla-central


.. dropdown:: wikipedia (BT, FF, CH, CU)
   :container: + anchor-id-wikipedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-wikipedia.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://en.wikipedia.org/wiki/Joe_Biden>`__
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: trunk
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: None
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None


.. dropdown:: yahoo-mail (BT, FF, CH, CU)
   :container: + anchor-id-yahoo-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: trunk
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: None
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None


.. dropdown:: youtube (BT, FF, CH, CU)
   :container: + anchor-id-youtube-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-youtube.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.youtube.com/watch?v=JrdEMERq8MA>`__
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-fis-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-fis-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-youtube-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: trunk
            * browsertime-tp6-firefox-youtube-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-youtube-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-fis-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-fis-e10s: None
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-fis-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-youtube-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None



Interactive
-----------
Browsertime tests that interact with the webpage. Includes responsiveness tests as they make use of this support for navigation. These form of tests allow the specification of browsertime commands through the test manifest.

.. dropdown:: cnn-nav (Navigates to cnn main page, then to the world sub-page.)
   :container: + anchor-id-cnn-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-cnn-nav.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "landing"], ["navigate", "https://www.cnn.com"], ["wait.byTime", 4000], ["measure.stop", ""], ["measure.start", "world"], ["click.byXpathAndWait", "/html/body/div[5]/div/div/header/div/div[1]/div/div[2]/nav/ul/li[2]/a"], ["wait.byTime", 1000], ["measure.stop", ""],
   * **test url**: `<https://www.cnn.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-cnn-nav-e10s: None
            * browsertime-responsiveness-chromium-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-cnn-nav-e10s: None
            * browsertime-responsiveness-chromium-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-cnn-nav-e10s: None
            * browsertime-responsiveness-chromium-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-responsiveness-chrome-cnn-nav-e10s: None
            * browsertime-responsiveness-chromium-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-responsiveness-firefox-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-cnn-nav-e10s: None
            * browsertime-responsiveness-chromium-cnn-nav-e10s: None
            * browsertime-responsiveness-firefox-cnn-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-cnn-nav-fis-e10s: mozilla-central


.. dropdown:: facebook-nav (Navigates to facebook, then the sub-pages friends, marketplace, groups.)
   :container: + anchor-id-facebook-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 90000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-facebook-nav.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "landing"], ["navigate", "https://www.facebook.com/"], ["wait.byTime", 5000], ["measure.stop", ""], ["measure.start", "marketplace"], ["navigate", "https://www.facebook.com/marketplace"], ["wait.byTime", 5000], ["measure.stop", ""], ["measure.start", "groups"], ["navigate", "https://www.facebook.com/groups/discover/"], ["wait.byTime", 5000], ["measure.stop", ""], ["measure.start", "friends"], ["navigate", "https://www.facebook.com/friends/"], ["wait.byTime", 5000], ["measure.stop", ""],
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-facebook-nav-e10s: None
            * browsertime-responsiveness-chromium-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-facebook-nav-e10s: None
            * browsertime-responsiveness-chromium-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-facebook-nav-e10s: None
            * browsertime-responsiveness-chromium-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-responsiveness-chrome-facebook-nav-e10s: None
            * browsertime-responsiveness-chromium-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-responsiveness-firefox-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-facebook-nav-e10s: None
            * browsertime-responsiveness-chromium-facebook-nav-e10s: None
            * browsertime-responsiveness-firefox-facebook-nav-e10s: mozilla-central
            * browsertime-responsiveness-firefox-facebook-nav-fis-e10s: mozilla-central


.. dropdown:: reddit-billgates-ama (Navigates from the Bill Gates AMA to the Reddit members section.)
   :container: + anchor-id-reddit-billgates-ama-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-reddit-billgates-ama.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "billg-ama"], ["navigate", "https://www.reddit.com/r/IAmA/comments/m8n4vt/im_bill_gates_cochair_of_the_bill_and_melinda/"], ["wait.byTime", 5000], ["measure.stop", ""], ["measure.start", "members"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div[3]/div[2]/div/div[1]/div/div[4]/div[1]/div"], ["wait.byTime", 1000], ["measure.stop", ""],
   * **test url**: `<https://www.reddit.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-ama-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-ama-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s: mozilla-central


.. dropdown:: reddit-billgates-post-1 (Navigates the `thisisbillgates` user starting at the main user page, then to the posts, comments, hot, and top sections.)
   :container: + anchor-id-reddit-billgates-post-1-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 90000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-reddit-billgates-post.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "billg"], ["navigate", "https://www.reddit.com/user/thisisbillgates/"], ["wait.byTime", 500], ["measure.stop", ""], ["measure.start", "posts"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div/div/div[2]/div[2]/div/div/div/a[2]"], ["wait.byTime", 500], ["measure.stop", ""], ["measure.start", "comments"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div/div/div[2]/div[2]/div/div/div/a[3]"], ["wait.byTime", 500], ["measure.stop", ""], ["wait.byTime", 500],
   * **test url**: `<https://www.reddit.com/user/thisisbillgates/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s: mozilla-central


.. dropdown:: reddit-billgates-post-2 (Navigates the `thisisbillgates` user starting at the main user page, then to the posts, comments, hot, and top sections.)
   :container: + anchor-id-reddit-billgates-post-2-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 90000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-reddit-billgates-post.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "billg"], ["navigate", "https://www.reddit.com/user/thisisbillgates/"], ["wait.byTime", 500], ["measure.stop", ""], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div/div/div[2]/div[2]/div/div/div/a[3]"], ["wait.byTime", 500], ["measure.start", "hot"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div/div/div[2]/div[4]/div[1]/div[1]/div[2]/a[2]"], ["wait.byTime", 500], ["measure.stop", ""], ["measure.start", "top"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div/div/div[2]/div[4]/div[1]/div[1]/div[2]/a[3]"], ["wait.byTime", 500], ["measure.stop", ""], ["wait.byTime", 500],
   * **test url**: `<https://www.reddit.com/user/thisisbillgates/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s: None
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s: mozilla-central
            * browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s: mozilla-central



Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms. (WX: WebExtension, BT: Browsertime, FF: Firefox, CH: Chrome, CU: Chromium, GV: Geckoview, RB: Refbrow, FE: Fenix, CH-M: Chrome mobile)

.. dropdown:: booking-sf (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-booking-sf-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://www.booking.com/hotel/us/edwardian-san-francisco.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: cnn (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-cnn-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-beta, trunk
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None


.. dropdown:: cnn-ampstories (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-cnn-ampstories-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: discord (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-discord-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://discordapp.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: expedia (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-expedia-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central


.. dropdown:: fashionbeans (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-fashionbeans-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://fashionbeans.com/article/coolest-menswear-stores-in-the-world>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: google-accounts (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-google-accounts-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://accounts.google.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: imdb-firefox (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-imdb-firefox-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://m.imdb.com/title/tt0083943/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: medium-article (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-medium-article-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://medium.com/s/coincidences-are-a-lie/could-america-have-also-been-the-birthplace-of-impressionism-cb3d31a2e22d>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: nytimes (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-nytimes-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central


.. dropdown:: people-article (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-people-article-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://people.com/amp-stories/royal-a-to-z/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: reddit-thread (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-reddit-thread-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://www.reddit.com/r/firefox/comments/7dkq03/its_been_a_while/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: rumble-fox (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-rumble-fox-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://rumble.com/v3c44t-foxes-jumping-on-my-trampoline.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: stackoverflow-question (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-stackoverflow-question-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://stackoverflow.com/questions/927358/how-do-i-undo-the-most-recent-commits-in-git>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: urbandictionary-define (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-urbandictionary-define-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://urbandictionary.com/define.php?term=awesome%20sauce>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:


.. dropdown:: wikia-marvel (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-wikia-marvel-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m, firefox, chrome, chromium
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **test url**: `<https://marvel.wikia.com/wiki/Black_Panther>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:



Mobile
------
Page-load performance test suite on Android. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, GV: Geckoview, RB: Refbrow, FE: Fenix, CH-M: Chrome mobile)

.. dropdown:: allrecipes (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-allrecipes-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-allrecipes.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.allrecipes.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-essential-fenix-allrecipes-e10s: None
            * browsertime-tp6m-essential-geckoview-allrecipes-e10s: None
            * browsertime-tp6m-essential-refbrow-allrecipes-e10s: None
            * browsertime-tp6m-live-chrome-m-allrecipes-e10s: None
            * browsertime-tp6m-live-fenix-allrecipes-e10s: None
            * browsertime-tp6m-live-geckoview-allrecipes-e10s: None


.. dropdown:: amazon (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-amazon-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-amazon.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-amazon-e10s: None
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-amazon-e10s: None
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-profiling-geckoview-amazon-e10s: mozilla-central
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-amazon-e10s: None
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-amazon-e10s: None
            * browsertime-tp6m-fenix-amazon-e10s: None
            * browsertime-tp6m-geckoview-amazon-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-e10s: None
            * browsertime-tp6m-live-fenix-amazon-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-e10s: None
            * browsertime-tp6m-refbrow-amazon-e10s: None


.. dropdown:: amazon-search (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-amazon-search-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-amazon-search.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com/s/ref=nb_sb_noss_2/139-6317191-5622045?url=search-alias%3Daps&field-keywords=mobile+phone>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-essential-fenix-amazon-search-e10s: None
            * browsertime-tp6m-essential-geckoview-amazon-search-e10s: None
            * browsertime-tp6m-essential-refbrow-amazon-search-e10s: None
            * browsertime-tp6m-live-chrome-m-amazon-search-e10s: None
            * browsertime-tp6m-live-fenix-amazon-search-e10s: None
            * browsertime-tp6m-live-geckoview-amazon-search-e10s: None


.. dropdown:: bing (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-bing-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-bing.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-e10s: None
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-e10s: None
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-e10s: None
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-chrome-m-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-e10s: None
            * browsertime-tp6m-fenix-bing-e10s: None
            * browsertime-tp6m-geckoview-bing-e10s: None
            * browsertime-tp6m-live-chrome-m-bing-e10s: None
            * browsertime-tp6m-live-fenix-bing-e10s: None
            * browsertime-tp6m-live-geckoview-bing-e10s: None
            * browsertime-tp6m-refbrow-bing-e10s: None


.. dropdown:: bing-search-restaurants (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-bing-search-restaurants-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-bing-search-restaurants.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/search?q=restaurants+in+exton+pa+19341>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-bing-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-bing-search-restaurants-e10s: None


.. dropdown:: booking (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-booking-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-booking.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.booking.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-booking-e10s: None
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-booking-e10s: None
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-booking-e10s: None
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-chrome-m-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-booking-e10s: None
            * browsertime-tp6m-fenix-booking-e10s: None
            * browsertime-tp6m-geckoview-booking-e10s: None
            * browsertime-tp6m-live-chrome-m-booking-e10s: None
            * browsertime-tp6m-live-fenix-booking-e10s: None
            * browsertime-tp6m-live-geckoview-booking-e10s: None
            * browsertime-tp6m-refbrow-booking-e10s: None


.. dropdown:: cnn (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-cnn-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-cnn.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-e10s: None
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-e10s: None
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-e10s: None
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-chrome-m-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-e10s: None
            * browsertime-tp6m-fenix-cnn-e10s: None
            * browsertime-tp6m-geckoview-cnn-e10s: None
            * browsertime-tp6m-live-chrome-m-cnn-e10s: None
            * browsertime-tp6m-live-fenix-cnn-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-e10s: None
            * browsertime-tp6m-refbrow-cnn-e10s: None


.. dropdown:: cnn-ampstories (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-cnn-ampstories-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-cnn-ampstories.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: autoland
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: autoland
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-fenix-cnn-ampstories-e10s: None
            * browsertime-tp6m-live-geckoview-cnn-ampstories-e10s: None
            * browsertime-tp6m-refbrow-cnn-ampstories-e10s: None


.. dropdown:: dailymail (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-dailymail-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-dailymail.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.dailymail.co.uk/sciencetech/article-9749081/Experts-say-Hubble-repair-despite-NASA-insisting-multiple-options-fix.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-fenix-dailymail-e10s: None
            * browsertime-tp6m-geckoview-dailymail-e10s: None
            * browsertime-tp6m-live-chrome-m-dailymail-e10s: None
            * browsertime-tp6m-live-fenix-dailymail-e10s: None
            * browsertime-tp6m-live-geckoview-dailymail-e10s: None
            * browsertime-tp6m-refbrow-dailymail-e10s: None


.. dropdown:: ebay-kleinanzeigen (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-ebay-kleinanzeigen-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-ebay-kleinanzeigen.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s: None


.. dropdown:: ebay-kleinanzeigen-search (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-ebay-kleinanzeigen-search-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-ebay-kleinanzeigen-search.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de/s-anzeigen/auf-zeit-wg-berlin/zimmer/c199-l3331>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s: None
            * browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s: None


.. dropdown:: espn (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-espn-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-espn.manifest
   * **playback version**: 6.0.2
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-espn-e10s: None
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-chrome-m-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-espn-e10s: None
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-chrome-m-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-espn-e10s: None
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-chrome-m-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-espn-e10s: None
            * browsertime-tp6m-essential-fenix-espn-e10s: None
            * browsertime-tp6m-essential-geckoview-espn-e10s: None
            * browsertime-tp6m-essential-refbrow-espn-e10s: None
            * browsertime-tp6m-live-chrome-m-espn-e10s: None
            * browsertime-tp6m-live-fenix-espn-e10s: None
            * browsertime-tp6m-live-geckoview-espn-e10s: None


.. dropdown:: facebook (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-facebook-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-facebook.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-facebook-e10s: None
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-facebook-e10s: None
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-facebook-e10s: None
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-facebook-e10s: None
            * browsertime-tp6m-essential-fenix-facebook-e10s: None
            * browsertime-tp6m-essential-geckoview-facebook-e10s: None
            * browsertime-tp6m-essential-refbrow-facebook-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-e10s: None
            * browsertime-tp6m-live-fenix-facebook-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-e10s: None


.. dropdown:: facebook-cristiano (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-facebook-cristiano-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-facebook-cristiano.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com/Cristiano>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-fenix-facebook-cristiano-e10s: None
            * browsertime-tp6m-live-geckoview-facebook-cristiano-e10s: None
            * browsertime-tp6m-refbrow-facebook-cristiano-e10s: None


.. dropdown:: google (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-google.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-google-e10s: None
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-chrome-m-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-google-e10s: None
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-chrome-m-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-google-e10s: None
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-chrome-m-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-google-e10s: None
            * browsertime-tp6m-essential-fenix-google-e10s: None
            * browsertime-tp6m-essential-geckoview-google-e10s: None
            * browsertime-tp6m-essential-refbrow-google-e10s: None
            * browsertime-tp6m-live-chrome-m-google-e10s: None
            * browsertime-tp6m-live-fenix-google-e10s: None
            * browsertime-tp6m-live-geckoview-google-e10s: None


.. dropdown:: google-maps (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-maps-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-google-maps.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/maps?force=pwa>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-fenix-google-maps-e10s: None
            * browsertime-tp6m-geckoview-google-maps-e10s: None
            * browsertime-tp6m-live-chrome-m-google-maps-e10s: None
            * browsertime-tp6m-live-fenix-google-maps-e10s: None
            * browsertime-tp6m-live-geckoview-google-maps-e10s: None
            * browsertime-tp6m-refbrow-google-maps-e10s: None


.. dropdown:: google-search-restaurants (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-search-restaurants-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-google-search-restaurants.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/search?q=restaurants+near+me>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-fenix-google-search-restaurants-e10s: None
            * browsertime-tp6m-live-geckoview-google-search-restaurants-e10s: None
            * browsertime-tp6m-refbrow-google-search-restaurants-e10s: None


.. dropdown:: imdb (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-imdb-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-imdb.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.imdb.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-imdb-e10s: None
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-imdb-e10s: None
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-imdb-e10s: None
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-chrome-m-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-imdb-e10s: None
            * browsertime-tp6m-fenix-imdb-e10s: None
            * browsertime-tp6m-geckoview-imdb-e10s: None
            * browsertime-tp6m-live-chrome-m-imdb-e10s: None
            * browsertime-tp6m-live-fenix-imdb-e10s: None
            * browsertime-tp6m-live-geckoview-imdb-e10s: None
            * browsertime-tp6m-refbrow-imdb-e10s: None


.. dropdown:: instagram (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-instagram-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-instagram.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.instagram.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-instagram-e10s: None
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-instagram-e10s: None
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-instagram-e10s: None
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-chrome-m-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-instagram-e10s: None
            * browsertime-tp6m-fenix-instagram-e10s: None
            * browsertime-tp6m-geckoview-instagram-e10s: None
            * browsertime-tp6m-live-chrome-m-instagram-e10s: None
            * browsertime-tp6m-live-fenix-instagram-e10s: None
            * browsertime-tp6m-live-geckoview-instagram-e10s: None
            * browsertime-tp6m-refbrow-instagram-e10s: None


.. dropdown:: microsoft-support (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-microsoft-support-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-microsoft-support.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://support.microsoft.com/en-us>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-essential-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-essential-geckoview-microsoft-support-e10s: None
            * browsertime-tp6m-essential-refbrow-microsoft-support-e10s: None
            * browsertime-tp6m-live-chrome-m-microsoft-support-e10s: None
            * browsertime-tp6m-live-fenix-microsoft-support-e10s: None
            * browsertime-tp6m-live-geckoview-microsoft-support-e10s: None


.. dropdown:: reddit (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-reddit-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-reddit.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.reddit.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-reddit-e10s: None
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-reddit-e10s: None
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-reddit-e10s: None
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-chrome-m-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-reddit-e10s: None
            * browsertime-tp6m-fenix-reddit-e10s: None
            * browsertime-tp6m-geckoview-reddit-e10s: None
            * browsertime-tp6m-live-chrome-m-reddit-e10s: None
            * browsertime-tp6m-live-fenix-reddit-e10s: None
            * browsertime-tp6m-live-geckoview-reddit-e10s: None
            * browsertime-tp6m-refbrow-reddit-e10s: None


.. dropdown:: sina (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-sina-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-sina.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.sina.com.cn/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-sina-e10s: None
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: mozilla-beta, trunk
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-sina-e10s: None
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: mozilla-beta, trunk
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-sina-e10s: None
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-sina-e10s: None
            * browsertime-tp6m-fenix-sina-e10s: None
            * browsertime-tp6m-geckoview-sina-e10s: None
            * browsertime-tp6m-refbrow-sina-e10s: None


.. dropdown:: stackoverflow (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-stackoverflow-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-stackoverflow.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://stackoverflow.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-live-chrome-m-stackoverflow-e10s: None
            * browsertime-tp6m-live-fenix-stackoverflow-e10s: None
            * browsertime-tp6m-live-geckoview-stackoverflow-e10s: None
            * browsertime-tp6m-refbrow-stackoverflow-e10s: None


.. dropdown:: web-de (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-web-de-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-web-de.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://web.de/magazine/politik/politologe-glaubt-grossen-koalition-herbst-knallen-33563566>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-web-de-e10s: None
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-web-de-e10s: None
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-web-de-e10s: None
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-chrome-m-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-web-de-e10s: None
            * browsertime-tp6m-fenix-web-de-e10s: None
            * browsertime-tp6m-geckoview-web-de-e10s: None
            * browsertime-tp6m-live-chrome-m-web-de-e10s: None
            * browsertime-tp6m-live-fenix-web-de-e10s: None
            * browsertime-tp6m-live-geckoview-web-de-e10s: None
            * browsertime-tp6m-refbrow-web-de-e10s: None


.. dropdown:: wikipedia (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-wikipedia-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-wikipedia.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://en.m.wikipedia.org/wiki/Main_Page>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-fenix-wikipedia-e10s: None
            * browsertime-tp6m-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-live-chrome-m-wikipedia-e10s: None
            * browsertime-tp6m-live-fenix-wikipedia-e10s: None
            * browsertime-tp6m-live-geckoview-wikipedia-e10s: None
            * browsertime-tp6m-refbrow-wikipedia-e10s: None


.. dropdown:: youtube (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-youtube-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-youtube.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://m.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-youtube-e10s: None
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-youtube-e10s: None
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6m-live-chrome-m-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-profiling-geckoview-youtube-e10s: mozilla-central
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-chrome-m-youtube-e10s: None
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-chrome-m-youtube-e10s: None
            * browsertime-tp6m-fenix-youtube-e10s: None
            * browsertime-tp6m-geckoview-youtube-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-e10s: None
            * browsertime-tp6m-live-fenix-youtube-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-e10s: None
            * browsertime-tp6m-refbrow-youtube-e10s: None


.. dropdown:: youtube-watch (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-youtube-watch-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-youtube-watch.manifest
   * **playback version**: 6.0.2
   * **test url**: `<https://www.youtube.com/watch?v=COU5T-Wafa4>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-g5-7-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: mozilla-beta, trunk
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-p2-8-0-arm7-qr/opt
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-p2-8-0-arm7-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-qr/opt
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None
      * test-android-hw-s7-8-0-android-aarch64-shippable-qr/opt
            * browsertime-tp6m-essential-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-essential-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-essential-geckoview-youtube-watch-e10s: None
            * browsertime-tp6m-essential-refbrow-youtube-watch-e10s: None
            * browsertime-tp6m-live-chrome-m-youtube-watch-e10s: None
            * browsertime-tp6m-live-fenix-youtube-watch-e10s: None
            * browsertime-tp6m-live-geckoview-youtube-watch-e10s: None



Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc. (FE: Fenix, GV: Geckoview, RB: Refbrow)

.. dropdown:: idle (FE, GV, RB)
   :container: + anchor-id-idle-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 1200000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete
   * **Test Task**:


.. dropdown:: idle-bg (FE, GV, RB)
   :container: + anchor-id-idle-bg-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow
   * **browsertime args**: --browsertime.scenario_time=60000 --browsertime.background_app=false
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 1200000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete
   * **Test Task**:


.. dropdown:: raptor-scn-power-idle-bg-fenix (FE)
   :container: + anchor-id-raptor-scn-power-idle-bg-fenix-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **background test**: true
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 660000
   * **scenario time**: 600000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete


.. dropdown:: raptor-scn-power-idle-bg-geckoview (GV)
   :container: + anchor-id-raptor-scn-power-idle-bg-geckoview-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **background test**: true
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 660000
   * **scenario time**: 600000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete


.. dropdown:: raptor-scn-power-idle-bg-refbrow (RB)
   :container: + anchor-id-raptor-scn-power-idle-bg-refbrow-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: refbrow
   * **background test**: true
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 660000
   * **scenario time**: 600000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete


.. dropdown:: raptor-scn-power-idle-fenix (FE)
   :container: + anchor-id-raptor-scn-power-idle-fenix-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 1200000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete


.. dropdown:: raptor-scn-power-idle-geckoview (GV)
   :container: + anchor-id-raptor-scn-power-idle-geckoview-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 1200000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete


.. dropdown:: raptor-scn-power-idle-refbrow (RB)
   :container: + anchor-id-raptor-scn-power-idle-refbrow-s

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: refbrow
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 1200000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete



Unittests
---------
These tests aren't used in standard testing, they are only used in the Raptor unit tests (they are similar to raptor-tp6 tests though).

.. dropdown:: raptor-tp6-unittest-amazon-firefox (FF)
   :container: + anchor-id-raptor-tp6-unittest-amazon-firefox-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms


.. dropdown:: raptor-tp6-unittest-facebook-firefox (FF)
   :container: + anchor-id-raptor-tp6-unittest-facebook-firefox-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms


.. dropdown:: raptor-tp6-unittest-google-firefox (FF)
   :container: + anchor-id-raptor-tp6-unittest-google-firefox-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__
   * **type**: pageload
   * **unit**: ms


.. dropdown:: raptor-tp6-unittest-youtube-firefox (FF)
   :container: + anchor-id-raptor-tp6-unittest-youtube-firefox-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms



