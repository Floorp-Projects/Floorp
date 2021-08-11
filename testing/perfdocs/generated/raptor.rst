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

We're in the process of migrating away from webextension to browsertime. Currently, raptor supports both of them, but at the end of the migration, the support for webextension will be removed.

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-ares6-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-ares6-e10s: None
            * browsertime-benchmark-chromium-ares6-e10s: None
            * browsertime-benchmark-firefox-ares6-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-ares6-fis-e10s: mozilla-central


.. dropdown:: assorted-dom (FF, CH, CU)
   :container: + anchor-id-assorted-dom-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-assorted-dom-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-assorted-dom-e10s: None
            * browsertime-benchmark-chromium-assorted-dom-e10s: None
            * browsertime-benchmark-firefox-assorted-dom-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-assorted-dom-fis-e10s: mozilla-central


.. dropdown:: jetstream2 (FF, CH, CU)
   :container: + anchor-id-jetstream2-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-central
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-central
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-central
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-jetstream2-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-jetstream2-e10s: None
            * browsertime-benchmark-chromium-jetstream2-e10s: None
            * browsertime-benchmark-firefox-jetstream2-e10s: mozilla-central
            * browsertime-benchmark-firefox-jetstream2-fis-e10s: mozilla-central


.. dropdown:: motionmark-animometer (FF, CH, CU)
   :container: + anchor-id-motionmark-animometer-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-animometer-e10s: None
            * browsertime-benchmark-chromium-motionmark-animometer-e10s: None
            * browsertime-benchmark-firefox-motionmark-animometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-animometer-fis-e10s: mozilla-central


.. dropdown:: motionmark-htmlsuite (FF, CH, CU)
   :container: + anchor-id-motionmark-htmlsuite-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-chromium-motionmark-htmlsuite-e10s: None
            * browsertime-benchmark-firefox-motionmark-htmlsuite-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s: mozilla-central


.. dropdown:: raptor-speedometer-geckoview (GV)
   :container: + anchor-id-raptor-speedometer-geckoview-b

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


.. dropdown:: raptor-youtube-playback-av1-sfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-av1-sfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-av1-sfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-av1-sfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-av1-sfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-av1-sfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-av1-sfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-av1-sfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-1080p30-firefox-b

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


.. dropdown:: raptor-youtube-playback-h264-sfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-h264-sfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-sfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-h264-sfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-sfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-h264-sfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-h264-sfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-h264-sfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-hfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-hfr-chrome-b

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: chrome
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


.. dropdown:: raptor-youtube-playback-hfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-hfr-fenix-b

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: fenix
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


.. dropdown:: raptor-youtube-playback-hfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-hfr-firefox-b

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
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
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-hfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-hfr-geckoview-b

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: geckoview
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


.. dropdown:: raptor-youtube-playback-v9-1080p30-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-v9-1080p30-firefox-b

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


.. dropdown:: raptor-youtube-playback-vp9-sfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-vp9-sfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-vp9-sfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-vp9-sfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-vp9-sfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-vp9-sfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-vp9-sfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-vp9-sfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-h264-sfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-widevine-h264-sfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-h264-sfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-widevine-h264-sfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-h264-sfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-widevine-h264-sfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-h264-sfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-widevine-h264-sfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-hfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-widevine-hfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-hfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-widevine-hfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-hfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-widevine-hfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-hfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-widevine-hfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
   * **expected**: pass
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **preferences**: {"media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true


.. dropdown:: raptor-youtube-playback-widevine-vp9-sfr-chrome (CH)
   :container: + anchor-id-raptor-youtube-playback-widevine-vp9-sfr-chrome-b

   * **alert threshold**: 2.0
   * **apps**: chrome
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


.. dropdown:: raptor-youtube-playback-widevine-vp9-sfr-fenix (FE)
   :container: + anchor-id-raptor-youtube-playback-widevine-vp9-sfr-fenix-b

   * **alert threshold**: 2.0
   * **apps**: fenix
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


.. dropdown:: raptor-youtube-playback-widevine-vp9-sfr-firefox (FF)
   :container: + anchor-id-raptor-youtube-playback-widevine-vp9-sfr-firefox-b

   * **alert threshold**: 2.0
   * **apps**: firefox
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


.. dropdown:: raptor-youtube-playback-widevine-vp9-sfr-geckoview (GV)
   :container: + anchor-id-raptor-youtube-playback-widevine-vp9-sfr-geckoview-b

   * **alert threshold**: 2.0
   * **apps**: geckoview
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


.. dropdown:: speedometer (FF, CH, CU, FE, GV, RB, CH-M)
   :container: + anchor-id-speedometer-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-speedometer-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-speedometer-e10s: None
            * browsertime-benchmark-chromium-speedometer-e10s: None
            * browsertime-benchmark-firefox-speedometer-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-speedometer-fis-e10s: mozilla-central


.. dropdown:: stylebench (FF, CH, CU)
   :container: + anchor-id-stylebench-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-stylebench-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-stylebench-e10s: None
            * browsertime-benchmark-chromium-stylebench-e10s: None
            * browsertime-benchmark-firefox-stylebench-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-stylebench-fis-e10s: mozilla-central


.. dropdown:: sunspider (FF, CH, CU)
   :container: + anchor-id-sunspider-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-sunspider-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-sunspider-e10s: None
            * browsertime-benchmark-chromium-sunspider-e10s: None
            * browsertime-benchmark-firefox-sunspider-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-sunspider-fis-e10s: mozilla-central


.. dropdown:: unity-webgl (FF, CH, CU, FE, RB, FE, CH-M)
   :container: + anchor-id-unity-webgl-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-unity-webgl-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-unity-webgl-e10s: None
            * browsertime-benchmark-chromium-unity-webgl-e10s: None
            * browsertime-benchmark-firefox-unity-webgl-e10s: autoland
            * browsertime-benchmark-firefox-unity-webgl-fis-e10s: mozilla-central


.. dropdown:: wasm-godot (FF, CH, CU)
   :container: + anchor-id-wasm-godot-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-godot-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s: mozilla-central


.. dropdown:: wasm-godot-baseline (FF)
   :container: + anchor-id-wasm-godot-baseline-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s: mozilla-central


.. dropdown:: wasm-godot-optimizing (FF)
   :container: + anchor-id-wasm-godot-optimizing-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s: mozilla-central


.. dropdown:: wasm-misc (FF, CH, CU)
   :container: + anchor-id-wasm-misc-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-chrome-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-chromium-wasm-misc-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s: mozilla-central


.. dropdown:: wasm-misc-baseline (FF)
   :container: + anchor-id-wasm-misc-baseline-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s: mozilla-central


.. dropdown:: wasm-misc-optimizing (FF)
   :container: + anchor-id-wasm-misc-optimizing-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s: mozilla-beta, trunk
            * browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s: mozilla-central


.. dropdown:: webaudio (FF, CH, CU)
   :container: + anchor-id-webaudio-b

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
      * test-linux1804-64-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-benchmark-firefox-webaudio-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-benchmark-chrome-webaudio-e10s: None
            * browsertime-benchmark-chromium-webaudio-e10s: None
            * browsertime-benchmark-firefox-webaudio-e10s: mozilla-beta, trunk
            * browsertime-benchmark-firefox-webaudio-fis-e10s: mozilla-central


.. dropdown:: youtube-playback (FF, GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-b

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
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-firefox-youtube-playback-av1-sfr-e10s: None


.. dropdown:: youtube-playback-h264-1080p30 (FF)
   :container: + anchor-id-youtube-playback-h264-1080p30-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-h264-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-hfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-hfr-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-hfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-v9-1080p30 (FF)
   :container: + anchor-id-youtube-playback-v9-1080p30-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-h264-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-h264-sfr-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-hfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-hfr-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-hfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s: mozilla-central


.. dropdown:: youtube-playback-widevine-vp9-sfr (FF , GV, FE, RB, CH)
   :container: + anchor-id-youtube-playback-widevine-vp9-sfr-b

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
      * test-linux1804-64-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s: mozilla-central
            * browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s: mozilla-central



Custom
------
Browsertime tests that use a custom pageload test script. These use the pageload type, but may have other intentions.

.. dropdown:: process-switch (Measures process switch time)
   :container: + anchor-id-process-switch-c

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
      * test-linux1804-64-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
            * browsertime-custom-firefox-process-switch-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-custom-firefox-process-switch-e10s: mozilla-central
            * browsertime-custom-firefox-process-switch-fis-e10s: mozilla-central


.. dropdown:: welcome (Measures pageload metrics for the first-install about:welcome page)
   :container: + anchor-id-welcome-c

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
      * test-linux1804-64-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: autoland, mozilla-central
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: autoland, mozilla-central
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: autoland, mozilla-central
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central
      * test-windows10-32-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
            * browsertime-first-install-firefox-welcome-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-first-install-firefox-welcome-e10s: autoland, mozilla-central
            * browsertime-first-install-firefox-welcome-fis-e10s: mozilla-central



Desktop
-------
Tests for page-load performance. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, FF: Firefox, CH: Chrome, CU: Chromium)

.. dropdown:: amazon (BT, FF, CH, CU)
   :container: + anchor-id-amazon-d

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
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-central
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
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None
            * browsertime-tp6-profiling-firefox-amazon-e10s: mozilla-central
            * browsertime-tp6-profiling-firefox-amazon-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
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
            * browsertime-tp6-live-firefox-amazon-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-amazon-e10s: None
            * browsertime-tp6-essential-chromium-amazon-e10s: None
            * browsertime-tp6-essential-firefox-amazon-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-amazon-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-amazon-e10s: None
            * browsertime-tp6-live-chromium-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-e10s: None
            * browsertime-tp6-live-firefox-amazon-fis-e10s: None


.. dropdown:: amazon-sec (BT, FF, CH, CU)
   :container: + anchor-id-amazon-sec-d

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
   * **playback pageset manifest**: mitm6-linux-firefox-amazon-sec.manifest
   * **playback version**: 6.0.2
   * **secondary url**: `<https://www.amazon.com/Acer-A515-46-R14K-Quad-Core-Processor-Backlit/dp/B08VKNVDDR/ref=sr_1_3?dchild=1&keywords=laptop&qid=1627047187&sr=8-3>`__
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk/opt
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-amazon-sec-fis-e10s: mozilla-central
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: trunk
            * browsertime-tp6-firefox-amazon-sec-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-amazon-sec-fis-e10s: mozilla-central
      * test-macosx1015-64-shippable/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-windows10-64-ref-hw-2017/opt
            * browsertime-tp6-firefox-amazon-sec-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-amazon-sec-e10s: None
            * browsertime-tp6-chromium-amazon-sec-e10s: None
            * browsertime-tp6-firefox-amazon-sec-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-amazon-sec-fis-e10s: mozilla-central


.. dropdown:: bing-search (BT, FF, CH, CU)
   :container: + anchor-id-bing-search-d

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
            * browsertime-tp6-live-firefox-bing-search-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
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
            * browsertime-tp6-live-firefox-bing-search-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-bing-search-e10s: None
            * browsertime-tp6-essential-chromium-bing-search-e10s: None
            * browsertime-tp6-essential-firefox-bing-search-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-bing-search-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-bing-search-e10s: None
            * browsertime-tp6-live-chromium-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-e10s: None
            * browsertime-tp6-live-firefox-bing-search-fis-e10s: None


.. dropdown:: buzzfeed (BT, FF, CH, CU)
   :container: + anchor-id-buzzfeed-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-buzzfeed.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.buzzfeed.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-buzzfeed-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-buzzfeed-e10s: None
            * browsertime-tp6-chromium-buzzfeed-e10s: None
            * browsertime-tp6-firefox-buzzfeed-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-buzzfeed-fis-e10s: mozilla-central


.. dropdown:: cnn (BT, FF, CH, CU)
   :container: + anchor-id-cnn-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-cnn.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None


.. dropdown:: ebay (BT, FF, CH, CU)
   :container: + anchor-id-ebay-d

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
   * **test url**: `<https://www.ebay.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
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
            * browsertime-tp6-live-firefox-ebay-e10s: None
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
            * browsertime-tp6-live-firefox-ebay-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-ebay-e10s: None
            * browsertime-tp6-live-firefox-ebay-e10s: None
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
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: None
            * browsertime-tp6-firefox-espn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-espn-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-espn-e10s: None
            * browsertime-tp6-chromium-espn-e10s: None
            * browsertime-tp6-firefox-espn-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-espn-fis-e10s: mozilla-central


.. dropdown:: expedia (BT, FF, CH, CU)
   :container: + anchor-id-expedia-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-expedia.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central


.. dropdown:: facebook (BT, FF, CH, CU)
   :container: + anchor-id-facebook-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
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
            * browsertime-tp6-live-firefox-facebook-e10s: None
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
            * browsertime-tp6-live-firefox-facebook-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-facebook-e10s: None
            * browsertime-tp6-live-firefox-facebook-e10s: None
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
            * browsertime-tp6-live-firefox-fandom-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
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
            * browsertime-tp6-live-firefox-fandom-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-fandom-e10s: None
            * browsertime-tp6-essential-chromium-fandom-e10s: None
            * browsertime-tp6-essential-firefox-fandom-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-fandom-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-fandom-e10s: None
            * browsertime-tp6-live-chromium-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-e10s: None
            * browsertime-tp6-live-firefox-fandom-fis-e10s: None


.. dropdown:: google-docs (BT, FF, CH, CU)
   :container: + anchor-id-google-docs-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-google-docs.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
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
            * browsertime-tp6-live-firefox-google-docs-e10s: None
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
            * browsertime-tp6-live-firefox-google-docs-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-google-docs-e10s: None
            * browsertime-tp6-live-firefox-google-docs-e10s: None
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
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-google-docs-canvas-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-google-docs-canvas-e10s: None
            * browsertime-tp6-chromium-google-docs-canvas-e10s: None
            * browsertime-tp6-firefox-google-docs-canvas-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-google-docs-canvas-fis-e10s: mozilla-central


.. dropdown:: google-mail (BT, FF, CH, CU)
   :container: + anchor-id-google-mail-d

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
            * browsertime-tp6-live-firefox-google-mail-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
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
            * browsertime-tp6-live-firefox-google-mail-e10s: None
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
            * browsertime-tp6-live-firefox-google-mail-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-google-mail-e10s: None
            * browsertime-tp6-live-firefox-google-mail-e10s: None
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
            * browsertime-tp6-live-firefox-google-search-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
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
            * browsertime-tp6-live-firefox-google-search-e10s: None
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
            * browsertime-tp6-live-firefox-google-search-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-google-search-e10s: None
            * browsertime-tp6-live-firefox-google-search-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-google-slides.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
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
            * browsertime-tp6-live-firefox-google-slides-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-google-slides-e10s: None
            * browsertime-tp6-essential-chromium-google-slides-e10s: None
            * browsertime-tp6-essential-firefox-google-slides-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-google-slides-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-google-slides-e10s: None
            * browsertime-tp6-live-chromium-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-e10s: None
            * browsertime-tp6-live-firefox-google-slides-fis-e10s: None


.. dropdown:: imdb (BT, FF, CH, CU)
   :container: + anchor-id-imdb-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-imdb.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
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
            * browsertime-tp6-live-firefox-imdb-e10s: None
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
            * browsertime-tp6-live-firefox-imdb-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-imdb-e10s: None
            * browsertime-tp6-live-firefox-imdb-e10s: None
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
            * browsertime-tp6-live-firefox-imgur-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
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
            * browsertime-tp6-live-firefox-imgur-e10s: None
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
            * browsertime-tp6-live-firefox-imgur-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-imgur-e10s: None
            * browsertime-tp6-live-firefox-imgur-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-instagram.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
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
            * browsertime-tp6-live-firefox-instagram-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-instagram-e10s: None
            * browsertime-tp6-essential-chromium-instagram-e10s: None
            * browsertime-tp6-essential-firefox-instagram-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-instagram-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-instagram-e10s: None
            * browsertime-tp6-live-chromium-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-e10s: None
            * browsertime-tp6-live-firefox-instagram-fis-e10s: None


.. dropdown:: linkedin (BT, FF, CH, CU)
   :container: + anchor-id-linkedin-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-linkedin.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
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
            * browsertime-tp6-live-firefox-linkedin-e10s: None
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
            * browsertime-tp6-live-firefox-linkedin-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-linkedin-e10s: None
            * browsertime-tp6-live-firefox-linkedin-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-microsoft.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
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
            * browsertime-tp6-live-firefox-microsoft-e10s: None
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
            * browsertime-tp6-live-firefox-microsoft-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-microsoft-e10s: None
            * browsertime-tp6-live-firefox-microsoft-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-netflix.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.netflix.com/title/80117263>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
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
            * browsertime-tp6-live-firefox-netflix-e10s: None
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
            * browsertime-tp6-live-firefox-netflix-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-netflix-e10s: None
            * browsertime-tp6-live-firefox-netflix-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-nytimes.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central


.. dropdown:: office (BT, FF, CH, CU)
   :container: + anchor-id-office-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-live-office.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://office.live.com/start/Word.aspx?omkt=en-US>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: None
            * browsertime-tp6-firefox-office-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-office-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-office-e10s: None
            * browsertime-tp6-chromium-office-e10s: None
            * browsertime-tp6-firefox-office-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-office-fis-e10s: mozilla-central


.. dropdown:: outlook (BT, FF, CH, CU)
   :container: + anchor-id-outlook-d

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
            * browsertime-tp6-live-firefox-outlook-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
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
            * browsertime-tp6-live-firefox-outlook-e10s: None
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
            * browsertime-tp6-live-firefox-outlook-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-outlook-e10s: None
            * browsertime-tp6-live-firefox-outlook-e10s: None
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
            * browsertime-tp6-live-firefox-paypal-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
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
            * browsertime-tp6-live-firefox-paypal-e10s: None
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
            * browsertime-tp6-live-firefox-paypal-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-paypal-e10s: None
            * browsertime-tp6-live-firefox-paypal-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-pinterest.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
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
            * browsertime-tp6-live-firefox-pinterest-e10s: None
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
            * browsertime-tp6-live-firefox-pinterest-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-pinterest-e10s: None
            * browsertime-tp6-live-firefox-pinterest-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-reddit.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
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
            * browsertime-tp6-live-firefox-reddit-e10s: None
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
            * browsertime-tp6-live-firefox-reddit-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-reddit-e10s: None
            * browsertime-tp6-live-firefox-reddit-e10s: None
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
   * **playback pageset manifest**: mitm5-linux-firefox-tumblr.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
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
            * browsertime-tp6-live-firefox-tumblr-e10s: None
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
            * browsertime-tp6-live-firefox-tumblr-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-tumblr-e10s: None
            * browsertime-tp6-live-firefox-tumblr-e10s: None
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
            * browsertime-tp6-live-firefox-twitch-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
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
            * browsertime-tp6-live-firefox-twitch-e10s: None
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
            * browsertime-tp6-live-firefox-twitch-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-twitch-e10s: None
            * browsertime-tp6-live-firefox-twitch-e10s: None
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
            * browsertime-tp6-live-firefox-twitter-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
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
            * browsertime-tp6-live-firefox-twitter-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-twitter-e10s: None
            * browsertime-tp6-essential-chromium-twitter-e10s: None
            * browsertime-tp6-essential-firefox-twitter-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-twitter-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-twitter-e10s: None
            * browsertime-tp6-live-chromium-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-e10s: None
            * browsertime-tp6-live-firefox-twitter-fis-e10s: None


.. dropdown:: wikia (BT, FF, CH, CU)
   :container: + anchor-id-wikia-d

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
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-wikia-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-wikia-e10s: None
            * browsertime-tp6-chromium-wikia-e10s: None
            * browsertime-tp6-firefox-wikia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-wikia-fis-e10s: mozilla-central


.. dropdown:: wikipedia (BT, FF, CH, CU)
   :container: + anchor-id-wikipedia-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-wikipedia.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
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
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-wikipedia-e10s: None
            * browsertime-tp6-essential-chromium-wikipedia-e10s: None
            * browsertime-tp6-essential-firefox-wikipedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-wikipedia-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-wikipedia-e10s: None
            * browsertime-tp6-live-chromium-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-e10s: None
            * browsertime-tp6-live-firefox-wikipedia-fis-e10s: None


.. dropdown:: yahoo-mail (BT, FF, CH, CU)
   :container: + anchor-id-yahoo-mail-d

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
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
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
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-essential-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-essential-firefox-yahoo-mail-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-yahoo-mail-e10s: None
            * browsertime-tp6-live-chromium-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-e10s: None
            * browsertime-tp6-live-firefox-yahoo-mail-fis-e10s: None


.. dropdown:: youtube (BT, FF, CH, CU)
   :container: + anchor-id-youtube-d

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
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-clang-trunk-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
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
            * browsertime-tp6-live-firefox-youtube-e10s: None
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
            * browsertime-tp6-live-firefox-youtube-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-youtube-e10s: None
            * browsertime-tp6-chromium-youtube-e10s: None
            * browsertime-tp6-firefox-youtube-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-youtube-fis-e10s: mozilla-central
            * browsertime-tp6-live-chrome-youtube-e10s: None
            * browsertime-tp6-live-chromium-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-e10s: None
            * browsertime-tp6-live-firefox-youtube-fis-e10s: None



Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms. (WX: WebExtension, BT: Browsertime, FF: Firefox, CH: Chrome, CU: Chromium, GV: Geckoview, RB: Refbrow, FE: Fenix, CH-M: Chrome mobile)

.. dropdown:: booking-sf (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-booking-sf-l

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
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-linux1804-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-linux1804-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1014-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-macosx1015-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-32-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-essential-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-essential-chrome-cnn-e10s: None
            * browsertime-tp6-essential-chromium-cnn-e10s: None
            * browsertime-tp6-essential-firefox-cnn-e10s: mozilla-beta, trunk
            * browsertime-tp6-essential-firefox-cnn-fis-e10s: mozilla-central
            * browsertime-tp6-live-sheriffed-firefox-cnn-e10s: None
            * browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s: None


.. dropdown:: cnn-ampstories (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-cnn-ampstories-l

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
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-expedia-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-expedia-e10s: None
            * browsertime-tp6-chromium-expedia-e10s: None
            * browsertime-tp6-firefox-expedia-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-expedia-fis-e10s: mozilla-central


.. dropdown:: fashionbeans (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-fashionbeans-l

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
      * test-linux1804-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
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
      * test-windows10-32-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-fis-e10s: None
      * test-windows10-64-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
      * test-windows10-64-ref-hw-2017-qr/opt
            * browsertime-tp6-firefox-nytimes-e10s: None
      * test-windows10-64-shippable-qr/opt
            * browsertime-tp6-chrome-nytimes-e10s: None
            * browsertime-tp6-chromium-nytimes-e10s: None
            * browsertime-tp6-firefox-nytimes-e10s: mozilla-beta, trunk
            * browsertime-tp6-firefox-nytimes-fis-e10s: mozilla-central


.. dropdown:: people-article (GV, FE, RB, CH-M, FF, CH, CU)
   :container: + anchor-id-people-article-l

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

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow
   * **background app**: true
   * **expected**: pass
   * **lower is better**: true
   * **measure**: fakeMeasure
   * **page cycles**: 1
   * **page timeout**: 1320000
   * **scenario time**: 600000
   * **test url**: `<about:blank>`__
   * **type**: scenario
   * **unit**: scenarioComplete
   * **Test Task**:


.. dropdown:: raptor-scn-power-idle-bg-fenix (FE)
   :container: + anchor-id-raptor-scn-power-idle-bg-fenix-s

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




Browsertime
***********

Browsertime is a harness for running performance tests, similar to Mozilla's Raptor testing framework. Browsertime is written in Node.js and uses Selenium WebDriver to drive multiple browsers including Chrome, Chrome for Android, Firefox, and Firefox for Android and GeckoView-based vehicles.

Source code:

- Our current Browsertime version uses the canonical repo.
- In-tree: https://searchfox.org/mozilla-central/source/tools/browsertime and https://searchfox.org/mozilla-central/source/taskcluster/scripts/misc/browsertime.sh

Running Locally
===============

**Prerequisites**

- A local mozilla repository clone with a `successful Firefox build <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`_ completed

Setup
=====

Note that if you are running Raptor-Browsertime then it will get installed automatically and also updates itself.

- Run ``./mach browsertime --setup``
- To check your setup, run ``./mach browsertime --check``, which will output something like:

::

    ffmpeg:   OK
    convert:  OK
    compare:  OK
    Pillow:   OK
    SSIM:     OK

- If ``ffmpeg`` is listed as FAIL, you might want to try this:

::

    cd ~/.mozbuild/browsertime/ffmpeg-4.1.1-macos64-static/bin
    chmod +x ffmpeg ffplay ffprobe

Now, try re-running ``./mach browsertime --check``, with ``ffmpeg`` hopefully fixed

* For other issues, see if ``./mach browsertime --setup --clobber`` fixes it, or deleting the ``~/.mozbuild/browsertime`` folder and running the aforementioned command.

* If you aren't running visual metrics, then failures in ``Pillow`` and ``SSIM`` can be ignored.

If ``convert`` and ``compare`` are also ``FAIL`` bugs which might further help are `Bug 1559168 <https://bugzilla.mozilla.org/show_bug.cgi?id=1559168>`_, `Bug 1559727 <https://bugzilla.mozilla.org/show_bug.cgi?id=1559168>`_, and `Bug 1574964 <https://bugzilla.mozilla.org/show_bug.cgi?id=1574964>`_, for starters. If none of the bugs are related to the issue, please file a bug ``Testing :: Raptor``.

* If you plan on running Browsertime on Android, your Android device must already be set up (see more below in the Android section)

Running on Firefox Desktop
==========================

Page-load tests
===============
There are two ways to run performance tests through browsertime listed below. **Note that ``./mach browsertime`` should not be used when debugging performance issues with profiles as it does not do symbolication.**

* Raptor-Browsertime (recommended):

::

  ./mach raptor --browsertime -t google-search

* Browsertime-"native":

::

    ./mach browsertime https://www.sitespeed.io --firefox.binaryPath '/Users/{userdir}/moz_src/mozilla-unified/obj-x86_64-apple-darwin18.7.0/dist/Nightly.app/Contents/MacOS/firefox'

Benchmark tests
===============
* Raptor-wrapped:

::

  ./mach raptor -t raptor-speedometer --browsertime

Running on Android
==================
Running on Raptor-Browsertime (recommended):
* Running on Fenix

::

  ./mach raptor --browsertime -t amazon --app fenix --binary org.mozilla.fenix

* Running on Geckoview

::

  ./mach raptor --browsertime -t amazon --app geckoview --binary org.mozilla.geckoview_example

Running on vanilla Browsertime:
* Running on Fenix/Firefox Preview

::

    ./mach browsertime --android --browser firefox --firefox.android.package org.mozilla.fenix.debug --firefox.android.activity org.mozilla.fenix.IntentReceiverActivity https://www.sitespeed.io

* Running on the GeckoView Example app

::

  ./mach browsertime --android --browser firefox https://www.sitespeed.io

Running on Google Chrome
========================
Chrome releases are tied to a specific version of ChromeDriver -- you will need to ensure the two are aligned.

There are two ways of doing this:

1. Download the ChromeDriver that matches the chrome you wish to run from https://chromedriver.chromium.org/ and specify the path:

::

  ./mach browsertime https://www.sitespeed.io -b chrome --chrome.chromedriverPath <PATH/TO/VERSIONED/CHROMEDRIVER>

2. Upgrade the ChromeDriver version in ``tools/browsertime/package-lock.json `` (see https://www.npmjs.com/package/@sitespeed.io/chromedriver for versions).
Run ``npm install``.

Launch vanilla Browsertime as follows:

::

  ./mach browsertime https://www.sitespeed.io -b chrome

Or for Raptor-Browsertime (use ``chrome`` for desktop, and ``chrome-m`` for mobile):

::

  ./mach raptor --browsertime -t amazon --app chrome --browsertime-chromedriver <PATH/TO/CHROMEDRIVER>

More Examples
=============

`Browsertime docs <https://github.com/mozilla/browsertime/tree/master/docs/examples>`_

Running Browsertime on Try
==========================
You can run all of our browsertime pageload tests through ``./mach try fuzzy --full``. We use chimera mode in these tests which means that both cold and warm pageload variants are running at the same time.

For example:

::

  ./mach try fuzzy -q "'g5 'imdb 'geckoview 'vismet '-wr 'shippable"

Retriggering Browsertime Visual Metrics Tasks
=============================================

You can retrigger Browsertime tasks just like you retrigger any other tasks from Treeherder (using the retrigger buttons, add-new-jobs, retrigger-multiple, etc.).

When you retrigger the Browsertime test task, it will trigger a new vismet task as well. If you retrigger a Browsertime vismet task, then it will cause the test task to be retriggered and a new vismet task will be produced from there. This means that both of these tasks are treated as "one" task when it comes to retriggering them.

There is only one path that still doesn't work for retriggering Browsertime tests and that happens when you use ``--rebuild X`` in a try push submission.

For details on how we previously retriggered visual metrics tasks see `VisualMetrics <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/VisualMetrics>`_ (this will stay here for a few months just in case).

Gecko Profiling with Browsertime
================================

To run gecko profiling using Raptor-Browsertime you can add the ``--gecko-profile`` flag to any command and you will get profiles from the test (with the profiler page opening in the browser automatically). This method also performs symbolication for you. For example:

::

  ./mach raptor --browsertime -t amazon --gecko-profile

Note that vanilla Browsertime does support Gecko Profiling but **it does not symbolicate the profiles** so it is **not recommended** to use for debugging performance regressions/improvements.

Upgrading Browsertime In-Tree
=============================
To upgrade the browsertime version used in-tree you can run, then commit the changes made to ``package.json`` and ``package-lock.json``:

::

  ./mach browsertime --update-upstream-url <TARBALL-URL>

Here is a sample URL that we can update to: https://github.com/sitespeedio/browsertime/tarball/89771a1d6be54114db190427dbc281582cba3d47

To test the upgrade, run a raptor test locally (with and without visual-metrics ``--browsertime-visualmetrics`` if possible) and test it on try with at least one test on desktop and mobile.

Finding the Geckodriver Being Used
==================================
If you're looking for the latest geckodriver being used there are two ways:
* Find the latest one from here: https://treeherder.mozilla.org/jobs?repo=mozilla-central&searchStr=geckodriver
* Alternatively, if you're trying to figure out which geckodriver a given CI task is using, you can click on the browsertime task in treeherder, and then click on the ``Task`` id in the bottom left of the pop-up interface. Then in the window that opens up, click on `See more` in the task details tab on the left, this will show you the dependent tasks with the latest toolchain-geckodriver being used. There's an Artifacts drop down on the right hand side for the toolchain-geckodriver task that you can find the latest geckodriver in.

If you're trying to test Browsertime with a new geckodriver, you can do either of the following:
* Request a new geckodriver build in your try run (i.e. through ``./mach try fuzzy``).
* Trigger a new geckodriver in a try push, then trigger the browsertime tests which will then use the newly built version in the try push.

Comparing Before/After Browsertime Videos
=========================================

We have some scripts that can produce side-by-side comparison videos for you of the worst pairing of videos. You can find the script here: https://github.com/gmierz/moz-current-tests#browsertime-side-by-side-video-comparisons

Once the side-by-side comparison is produced, the video on the left is the old/base video, and the video on the right is the new video.

WebExtension
************
WebExtension Page-Load Tests
============================

Page-load tests involve loading a specific web page and measuring the load performance (i.e. `time-to-first-non-blank-paint <https://wiki.mozilla.org/TestEngineering/Performance/Glossary#First_Non-Blank_Paint_.28fnbpaint.29>`_, first-contentful-paint, `dom-content-flushed <https://wiki.mozilla.org/TestEngineering/Performance/Glossary#DOM_Content_Flushed_.28dcf.29>`_).

For page-load tests by default, instead of using live web pages for performance testing, Raptor uses a tool called `Mitmproxy <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/Mitmproxy>`_. Mitmproxy allows us to record and playback test pages via a local Firefox proxy. The Mitmproxy recordings are stored on `tooltool <https://github.com/mozilla/build-tooltool>`_ and are automatically downloaded by Raptor when they are required for a test. Raptor uses mitmproxy via the `mozproxy <https://searchfox.org/mozilla-central/source/testing/mozbase/mozproxy>`_ package.

There are two different types of Raptor page-load tests: warm page-load and cold page-load.

Warm Page-Load
--------------
For warm page-load tests, the browser is just started up once; so the browser is warm on each page-load.

**Raptor warm page-load test process when running on Firefox/Chrome/Chromium desktop:**

* A new browser profile is created
* The desktop browser is started up
* Post-startup browser settle pause of 30 seconds
* A new tab is opened
* The test URL is loaded; measurements taken
* The tab is reloaded 24 more times; measurements taken each time
* The measurements from the first page-load are not included in overall results metrics b/c of first load noise; however they are listed in the JSON artifacts

**Raptor warm page-load test process when running on Firefox android browser apps:**

* The android app data is cleared (via ``adb shell pm clear firefox.app.binary.name``)
* The new browser profile is copied onto the android device SD card
* The Firefox android app is started up
* Post-startup browser settle pause of 30 seconds
* The test URL is loaded; measurements taken
* The tab is reloaded 14 more times; measurements taken each time
* The measurements from the first page-load are not included in overall results metrics b/c of first load noise; however they are listed in the JSON artifacts

Cold Page-Load
--------------
For cold page-load tests, the browser is shut down and restarted between page load cycles, so the browser is cold on each page-load. This is what happens for Raptor cold page-load tests:

**Raptor cold page-load test process when running on Firefox/Chrome/Chromium desktop:**

* A new browser profile is created
* The desktop browser is started up
* Post-startup browser settle pause of 30 seconds
* A new tab is opened
* The test URL is loaded; measurements taken
* The tab is closed
* The desktop browser is shut down
* Entire process is repeated for the remaining browser cycles (25 cycles total)
* The measurements from all browser cycles are used to calculate overall results

**Raptor cold page-load test process when running on Firefox Android browser apps:**

* The Android app data is cleared (via ``adb shell pm clear firefox.app.binary.name``)
* A new browser profile is created
* The new browser profile is copied onto the Android device SD card
* The Firefox Android app is started up
* Post-startup browser settle pause of 30 seconds
* The test URL is loaded; measurements taken
* The Android app is shut down
* Entire process is repeated for the remaining browser cycles (15 cycles total)
* Note that the SSL cert DB is only created once (browser cycle 1) and copied into the profile for each additional browser cycle, thus not having to use the 'certutil' tool and re-created the db on each cycle
* The measurements from all browser cycles are used to calculate overall results

Using Live Sites
----------------
It is possible to use live web pages for the page-load tests instead of using the mitmproxy recordings. To do this, add ``--live`` to your command line or select one of the 'live' variants when running ``./mach try fuzzy --full``.

Disabling Alerts
----------------
It is possible to disable alerting for all our performance tests. Open the target test manifest such as the raptor-tp6*.ini file (`Raptor tests folder <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/tests>`_), and make sure there are no ``alert_on`` specifications.

When it's removed there will no longer be a ``shouldAlert`` field in the output Perfherder data (you can find the `schema here <https://searchfox.org/mozilla-central/source/testing/mozharness/external_tools/performance-artifact-schema.json#68,165>`_). As long as ``shouldAlert`` is not in the data, no alerts will be generated. If you need to also disable code sheriffing for the test, then you need to change the tier of the task to 3.

High value tests
----------------

We have a notion of **high-value** tests in performance testing. These are chosen based on how many alerts they can catch using `this script <https://github.com/gmierz/moz-current-tests/blob/master/high-value-tests/generate_high_value_tests.py>`_ along with `a redash query <https://github.com/gmierz/moz-current-tests/blob/master/high-value-tests/sql_query.txt>`_. The lists below are the minimum set of test pages we should run to catch as many alerts as we can.

**Desktop**

Last updated: June 2021

   - amazon
   - bing
   - cnn
   - fandom
   - gslides
   - instagram
   - twitter
   - wikipedia
   - yahoo-mail

**Mobile**

Last updated: November 2020

    - allrecipes
    - amazon-search
    - espn
    - facebook
    - google-search
    - microsoft-support
    - youtube-watch

WebExtension Benchmark Tests
============================

Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI.

Scenario Tests
==============

Currently, there are three subtypes of Raptor-run "scenario" tests, all on (and only on) Android:

#. **power-usage tests**
#. **memory-usage tests**
#. **CPU-usage tests**

For a combined-measurement run with distinct Perfherder output for each measurement type, you can do:

::

  ./mach raptor --test raptor-scn-power-idle-bg-fenix --app fenix --binary org.mozilla.fenix.performancetest --host 10.0.0.16 --power-test --memory-test --cpu-test

Each measurement subtype (power-, memory-, and cpu-usage) will have a corresponding PERFHERDER_DATA blob:

::

    22:31:05     INFO -  raptor-output Info: PERFHERDER_DATA: {"framework": {"name": "raptor"}, "suites": [{"name": "raptor-scn-power-idle-bg-fenix-cpu", "lowerIsBetter": true, "alertThreshold": 2.0, "value": 0, "subtests": [{"lowerIsBetter": true, "unit": "%", "name": "cpu-browser_cpu_usage", "value": 0, "alertThreshold": 2.0}], "type": "cpu", "unit": "%"}]}
    22:31:05     INFO -  raptor-output Info: cpu results can also be found locally at: /Users/sdonner/moz_src/mozilla-unified/testing/mozharness/build/raptor-cpu.json

(repeat for power, memory snippets)

Power-Use Tests (Android)
-------------------------
Prerequisites
^^^^^^^^^^^^^


#. rooted (i.e. superuser-capable), bootloader-unlocked Moto G5 or Google Pixel 2: internal (for now) `test-device setup doc. <https://docs.google.com/document/d/1XQLtvVM2U3h1jzzzpcGEDVOp4jMECsgLYJkhCfAwAnc/edit>`_
#. set up to run Raptor from a Firefox source tree (see `Running Locally <https://wiki.mozilla.org/Performance_sheriffing/Raptor#Running_Locally>`_
#. `GeckoView-bootstrapped <https://wiki.mozilla.org/Performance_sheriffing/Raptor#Running_on_the_Android_GeckoView_Example_App>`_ environment

**Raptor power-use measurement test process when running on Firefox Android browser apps:**

* The Android app data is cleared, via:

::

  adb shell pm clear firefox.app.binary.name


* The new browser profile is copied onto the Android device's SD card
* We set ``scenario_time`` to **20 minutes** (1200000 milliseconds), and ``page_timeout`` to '22 minutes' (1320000 milliseconds)

**It's crucial that ``page_timeout`` exceed ``scenario_time``; if not, measurement tests will fail/bail early**

* We launch the {Fenix, GeckoView, Reference Browser} on-Android app
* Post-startup browser settle pause of 30 seconds
* On Fennec only, a new browser tab is created (other Firefox apps use the single/existing tab)
* Power-use/battery-level measurements (app-specific measurements) are taken, via:

::

  adb shell dumpsys batterystats


* Raw power-use measurement data is listed in the perfherder-data.json/raptor.json artifacts

In the Perfherder dashboards for these power usage tests, all data points have milli-Ampere-hour units, with a lower value being better.
Proportional power usage is the total power usage of hidden battery sippers that is proportionally "smeared"/distributed across all open applications.

Running Scenario Tests Locally
------------------------------

To run on a tethered phone via USB from a macOS host, on:

Fenix
^^^^^


::

  ./mach raptor --test raptor-scn-power-idle-fenix --app fenix --binary org.mozilla.fenix.performancetest --power-test --host 10.252.27.96

GeckoView
^^^^^^^^^


::

  ./mach raptor --test raptor-scn-power-idle-geckoview --app geckoview --binary org.mozilla.geckoview_example --power-test --host 10.252.27.96

Reference Browser
^^^^^^^^^^^^^^^^^


::

  ./mach raptor --test raptor-scn-power-idle-refbrow --app refbrow --binary org.mozilla.reference.browser.raptor --power-test --host 10.252.27.96

**NOTE:**
*it is important that you include* ``--power-test``, *when running power-usage measurement tests, as that will help ensure that local test-measurement data doesn't accidentally get submitted to Perfherder*

Writing New Tests
-----------------

Pushing to Try server
---------------------
As an example, a relatively good cross-sampling of builds can be seen in https://hg.mozilla.org/try/rev/6c07631a0c2bf56b51bb82fd5543d1b34d7f6c69.

* Include both G5 Android 7 (hw-g5-7-0-arm7-api-16/) *and* Pixel 2 Android 8 (p2-8-0-android-aarch64/) target platforms
* pgo builds tend to be -- from my limited empirical evidence -- about 10 - 15 minutes longer to complete than their opt counterparts

Dashboards
----------

See `performance results <https://wiki.mozilla.org/TestEngineering/Performance/Results>`_ for our various dashboards.

Running WebExtension Locally
============================

Prerequisites
-------------

In order to run Raptor on a local machine, you need:

* A local mozilla repository clone with a `successful Firefox build <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`_ completed
* Git needs to be in the path in the terminal/window in which you build Firefox / run Raptor, as Raptor uses Git to check-out a local copy for some of the performance benchmarks' sources.
* If you plan on running Raptor tests on Google Chrome, you need a local install of Google Chrome and know the path to the chrome binary
* If you plan on running Raptor on Android, your Android device must already be set up (see more below in the Android section)

Getting a List of Raptor Tests
------------------------------

To see which Raptor performance tests are currently available on all platforms, use the 'print-tests' option, e.g.:

::

  $ ./mach raptor --print-tests

That will output all available tests on each supported app, as well as each subtest available in each suite (i.e. all the pages in a specific page-load tp6* suite).

Running on Firefox
------------------

To run Raptor locally, just build Firefox and then run:

::

  $ ./mach raptor --test <raptor-test-name>

For example, to run the raptor-tp6 pageload test locally, just use:

::

  $ ./mach raptor --test raptor-tp6-1

You can run individual subtests too (i.e. a single page in one of the tp6* suites). For example, to run the amazon page-load test on Firefox:

::

  $ ./mach raptor --test raptor-tp6-amazon-firefox

Raptor test results will be found locally in <your-repo>/testing/mozharness/build/raptor.json.

Running on the Android GeckoView Example App
--------------------------------------------

When running Raptor tests on a local Android device, Raptor is expecting the device to already be set up and ready to go.

First, ensure your local host machine has the Android SDK/Tools (i.e. ADB) installed. Check if it is already installed by attaching your Android device to USB and running:

::

  $ adb devices

If your device serial number is listed, then you're all set. If ADB is not found, you can install it by running (in your local mozilla-development repo):

::

  $ ./mach bootstrap

Then, in bootstrap, select the option for "Firefox for Android Artifact Mode," which will install the required tools (no need to do an actual build).

Next, make sure your Android device is ready to go. Local Android-device prerequisites are:

* Device is `rooted <https://docs.google.com/document/d/1XQLtvVM2U3h1jzzzpcGEDVOp4jMECsgLYJkhCfAwAnc/edit>`_

Note: If you are using Magisk to root your device, use `version 17.3 <https://github.com/topjohnwu/Magisk/releases/tag/v17.3>`_

* Device is in 'superuser' mode

* The geckoview example app is already installed on the device (from ``./mach bootstrap``, above). Download the geckoview_example.apk from the appropriate `android build on treeherder <https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&searchStr=android%2Cbuild>`_, then install it on your device, i.e.:

::

  $ adb install -g ../Downloads/geckoview_example.apk

The '-g' flag will automatically set all application permissions ON, which is required.

Note, when the Gecko profiler should be run, or a build with build symbols is needed, then use a `Nightly build of geckoview_example.apk <https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&searchStr=nightly%2Candroid>`_.

When updating the geckoview example app, you MUST uninstall the existing one first, i.e.:

::

  $ adb uninstall org.mozilla.geckoview_example

Once your Android device is ready, and attached to local USB, from within your local mozilla repo use the following command line to run speedometer:

::

  $ ./mach raptor --test raptor-speedometer --app=geckoview --binary="org.mozilla.geckoview_example"

Note: Speedometer on Android GeckoView is currently running on two devices in production - the Google Pixel 2 and the Moto G5 - therefore it is not guaranteed that it will run successfully on all/other untested android devices. There is an intermittent failure on the Moto G5 where speedometer just stalls (`Bug 1492222 <https://bugzilla.mozilla.org/show_bug.cgi?id=1492222>`_).

To run a Raptor page-load test (i.e. tp6m-1) on the GeckoView Example app, use this command line:

::

  $ ./mach raptor --test raptor-tp6m-1 --app=geckoview --binary="org.mozilla.geckoview_example"

A couple notes about debugging:

* Raptor browser-extension console messages *do* appear in adb logcat via the GeckoConsole - so this is handy:

::

  $ adb logcat | grep GeckoConsole

* You can also debug Raptor on Android using the Firefox WebIDE; click on the Android device listed under "USB Devices" and then "Main Process" or the 'localhost: Speedometer.." tab process

Raptor test results will be found locally in <your-repo>/testing/mozharness/build/raptor.json.

Running on Google Chrome
------------------------

To run Raptor locally on Google Chrome, make sure you already have a local version of Google Chrome installed, and then from within your mozilla-repo run:

::

  $ ./mach raptor --test <raptor-test-name> --app=chrome --binary="<path to google chrome binary>"

For example, to run the raptor-speedometer benchmark on Google Chrome use:

::

  $ ./mach raptor --test raptor-speedometer --app=chrome --binary="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome

Raptor test results will be found locally in <your-repo>/testing/mozharness/build/raptor.json.

Page-Timeouts
-------------

On different machines the Raptor tests will run at different speeds. The default page-timeout is defined in each Raptor test INI file. On some machines you may see a test failure with a 'raptor page-timeout' which means the page-load timed out, or the benchmark test iteration didn't complete, within the page-timeout limit.

You can override the default page-timeout by using the --page-timeout command-line arg. In this example, each test page in tp6-1 will be given two minutes to load during each page-cycle:

::

  ./mach raptor --test raptor-tp6-1 --page-timeout 120000

If an iteration of a benchmark test is not finishing within the allocated time, increase it by:

::

  ./mach raptor --test raptor-speedometer --page-timeout 600000

Page-Cycles
-----------

Page-cycles is the number of times a test page is loaded (for page-load tests); for benchmark tests, this is the total number of iterations that the entire benchmark test will be run. The default page-cycles is defined in each Raptor test INI file.

You can override the default page-cycles by using the --page-cycles command-line arg. In this example, the test page will only be loaded twice:

::

  ./mach raptor --test raptor-tp6-google-firefox --page-cycles 2

Running Page-Load Tests on Live Sites
-------------------------------------
To use live pages instead of page recordings, just edit the `Raptor tp6* test INI <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/tests>`_ file and add the following attribute either at the top (for all pages in the suite) or under an individual page/subtest heading:

  use_live_pages = true

With that setting, Raptor will not start the playback tool (i.e. Mitmproxy) and will not turn on the corresponding browser proxy, therefore forcing the test page to load live.

Running Raptor on Try
=====================

Raptor tests can be run on `try <https://treeherder.mozilla.org/#/jobs?repo=try>`_ on both Firefox and Google Chrome. (Raptor pageload-type tests are not supported on Google Chrome yet, as mentioned above).

**Note:** Raptor is currently 'tier 2' on `Treeherder <https://treeherder.mozilla.org/#/jobs?repo=try>`_, which means to see the Raptor test jobs you need to ensure 'tier 2' is selected / turned on in the Treeherder 'Tiers' menu.

The easiest way to run Raptor tests on try is to use mach try fuzzy:

::

  $ ./mach try fuzzy --full

Then type 'raptor' and select which Raptor tests (and on what platforms) you wish to run.

To see the Raptor test results on your try run:

#. In treeherder select one of the Raptor test jobs (i.e. 'sp' in 'Rap-e10s', or 'Rap-C-e10s')
#. Below the jobs, click on the "Performance" tab; you'll see the aggregated results listed
#. If you wish to see the raw replicates, click on the "Job Details" tab, and select the "perfherder-data.json" artifact

Raptor Hardware in Production
-----------------------------

The Raptor performance tests run on dedicated hardware (the same hardware that the Talos performance tests use). See the `performance platforms <https://wiki.mozilla.org/TestEngineering/Performance/Platforms>`_ for more details.

Profiling Raptor Jobs
=====================

Raptor tests are able to create Gecko profiles which can be viewed in `profiler.firefox.com. <https://profiler.firefox.com/>`_ This is currently only supported when running Raptor on Firefox desktop.

Nightly Profiling Jobs in Production
------------------------------------
We have Firefox desktop Raptor jobs with Gecko-profiling enabled running Nightly in production on Mozilla Central (on Linux64, Win10, and OSX). This provides a steady cache of Gecko profiles for the Raptor tests. Search for the `"Rap-Prof" treeherder group on Mozilla Central <https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&searchStr=Rap-Prof>`_.

Profiling Locally
-----------------

To tell Raptor to create Gecko profiles during a performance test, just add the '--gecko-profile' flag to the command line, i.e.:

::

  $ ./mach raptor --test raptor-sunspider --gecko-profile

When the Raptor test is finished, you will be able to find the resulting gecko profiles (ZIP) located locally in:

     mozilla-central/testing/mozharness/build/blobber_upload_dir/

Note: While profiling is turned on, Raptor will automatically reduce the number of pagecycles to 3. If you wish to override this, add the --page-cycles argument to the raptor command line.

Raptor will automatically launch Firefox and load the latest Gecko profile in `profiler.firefox.com <https://profiler.firefox.com>`_. To turn this feature off, just set the DISABLE_PROFILE_LAUNCH=1 env var.

If auto-launch doesn't work for some reason, just start Firefox manually and browse to `profiler.firefox.com <https://profiler.firefox.com>`_, click on "Browse" and select the Raptor profile ZIP file noted above.

If you're on Windows and want to profile a Firefox build that you compiled yourself, make sure it contains profiling information and you have a symbols zip for it, by following the `directions on MDN <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Profiling_with_the_Built-in_Profiler_and_Local_Symbols_on_Windows#Profiling_local_talos_runs>`_.

Profiling on Try Server
-----------------------

To turn on Gecko profiling for Raptor test jobs on try pushes, just add the '--gecko-profile' flag to your try push i.e.:

::

  $ ./mach try fuzzy --gecko-profile

Then select the Raptor test jobs that you wish to run. The Raptor jobs will be run on try with profiling included. While profiling is turned on, Raptor will automatically reduce the number of pagecycles to 2.

See below for how to view the gecko profiles from within treeherder.

Customizing the profiler
------------------------
If the default profiling options are not enough, and further information is needed the gecko profiler can be customized.

Enable profiling of additional threads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In some cases it will be helpful to also measure threads which are not part of the default set. Like the **MediaPlayback** thread. This can be accomplished by using:

#. the **gecko_profile_threads** manifest entry, and specifying the thread names as comma separated list
#. the **--gecko-profile-thread** argument for **mach** for each extra thread to profile

Add Profiling to Previously Completed Jobs
------------------------------------------

Note: You might need treeherder 'admin' access for the following.

Gecko profiles can now be created for Raptor performance test jobs that have already completed in production (i.e. mozilla-central) and on try. To repeat a completed Raptor performance test job on production or try, but add gecko profiling, do the following:

#. In treeherder, select the symbol for the completed Raptor test job (i.e. 'ss' in 'Rap-e10s')
#. Below, and to the left of the 'Job Details' tab, select the '...' to show the menu
#. On the pop-up menu, select 'Create Gecko Profile'

The same Raptor test job will be repeated but this time with gecko profiling turned on. A new Raptor test job symbol will be added beside the completed one, with a '-p' added to the symbol name. Wait for that new Raptor profiling job to finish. See below for how to view the gecko profiles from within treeherder.

Viewing Profiles on Treeherder
------------------------------
When the Raptor jobs are finished, to view the gecko profiles:

#. In treeherder, select the symbol for the completed Raptor test job (i.e. 'ss' in 'Rap-e10s')
#. Click on the 'Job Details' tab below
#. The Raptor profile ZIP files will be listed as job artifacts;
#. Select a Raptor profile ZIP artifact, and click the 'view in Firefox Profiler' link to the right

Recording Pages for Raptor Pageload Tests
=========================================

Raptor pageload tests ('tp6' and 'tp6m' suites) use the `Mitmproxy <https://mitmproxy.org/>`__ tool to record and play back page archives. For more information on creating new page playback archives, please see `Raptor and Mitmproxy <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/Mitmproxy>`__.

Performance Tuning for Android devices
======================================

When the test is run against Android, Raptor executes a series of performance tuning commands over the ADB connection.

Device agnostic:

* memory bus
* device remain on when on USB power
* virtual memory (swappiness)
* services (thermal throttling, cpu throttling)
* i/o scheduler

Device specific:

* cpu governor
* cpu minimum frequency
* gpu governor
* gpu minimum frequency

For a detailed list of current tweaks, please refer to `this <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/raptor.py#676>`_ Searchfox page.

Raptor Test List
****************

Currently the following Raptor tests are available. Note: Check the test details below to see which browser (i.e. Firefox, Google Chrome, Android) each test is supported on.

Page-Load Tests
===============

Raptor page-load test documentation is generated by `PerfDocs <https://firefox-source-docs.mozilla.org/code-quality/lint/linters/perfdocs.html>`_ and available in the `Firefox Source Docs <https://firefox-source-docs.mozilla.org/testing/perfdocs/raptor.html>`_.

Benchmark Tests
===============

assorted-dom
------------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* TODO

motionmark-animometer, motionmark-htmlsuite
-------------------------------------------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* measuring: benchmark measuring the time to animate complex scenes
* summarization:
** subtest: FPS from the subtest, each subtest is run for 15 seconds, repeat this 5 times and report the median value
** suite: we take a geometric mean of all the subtests (9 for animometer, 11 for html suite)

speedometer
-----------

* contact: :selena
* type: benchmark
* browsers: Firefox desktop, Chrome desktop, Firefox Android Geckoview
* measuring: responsiveness of web applications
* reporting: runs/minute score
* data: there are 16 subtests in Speedometer; each of these are made up of 9 internal benchmarks.
* summarization:
* * subtest: For all of the 16 subtests, we collect `a summed of all their internal benchmark results <https://searchfox.org/mozilla-central/source/third_party/webkit/PerformanceTests/Speedometer/resources/benchmark-report.js#66-67>`_ for each of them. To obtain a single score per subtest, we take `a median of the replicates <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/output.py#427-470>`_.
* * score: `geometric mean of the 16 subtest metrics (along with some special corrections) <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/output.py#317-330>`_.

This is the `Speedometer v1.0 <http://browserbench.org/Speedometer/>`_ JavaScript benchmark taken verbatim and slightly modified to work with the Raptor harness.

stylebench
----------

* contact: :emilio
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* measuring: speed of dynamic style recalculation
* reporting: runs/minute score

sunspider
---------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* TODO

unity-webgl
-----------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop, Firefox Android Geckoview
* TODO

youtube-playback
----------------

* contact: ?
* type: benchmark
* details: `YouTube playback performance <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/Youtube_playback_performance>`_
* browsers: Firefox desktop, Firefox Android Geckoview
* measuring: media streaming playback performance (dropped video frames)
* reporting: For each video the number of dropped and decoded frames, as well as its percentage value is getting recorded. The overall reported result is the mean value of dropped video frames across all tested video files.
* data: Given the size of the used media files those tests are currently run as live site tests, and are kept up-to-date via the `perf-youtube-playback <https://github.com/mozilla/perf-youtube-playback/>`_ repository on Github.
* test INI: `raptor-youtube-playback.ini <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/tests/raptor-youtube-playback.ini>`_

This are the `Playback Performance Tests <https://ytlr-cert.appspot.com/2019/main.html?test_type=playbackperf-test>`_ benchmark taken verbatim and slightly modified to work with the Raptor harness.

wasm-misc, wasm-misc-baseline, wasm-misc-ion
--------------------------------------------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* TODO

wasm-godot, wasm-godot-baseline, wasm-godot-ion
-----------------------------------------------

* contact: ?
* type: benchmark
* browsers: Firefox desktop only
* TODO

webaudio
--------

* contact: ?
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* TODO

Scenario Tests
==============

This test type runs browser tests that use idle pages for a specified amount of time to gather resource usage information such as power usage. The pages used for testing do not need to be recorded with mitmproxy.

When creating a new scenario test, ensure that the `page-timeout` is greater than the `scenario-time` to make sure raptor doesn't exit the test before the scenario timer ends.

This test type can also be used for specialized tests that require communication with the control-server to do things like sending the browser to the background for X minutes.

Power-Usage Measurement Tests
-----------------------------
These Android power measurement tests output 3 different PERFHERDER_DATA entries. The first contains the power usage of the test itself, the second contains the power usage of the android OS (named os-baseline) over the course of 1 minute, and the third (the name is the test name with '%change-power' appended to it) is a combination of these two measures which shows the percentage increase in power consumption when the test is run, in comparison to when it is not running. In these perfherder data blobs, we provide power consumption attributed to the cpu, wifi, and screen in Milli-ampere-hours (mAh).

raptor-scn-power-idle
^^^^^^^^^^^^^^^^^^^^^

* contact: stephend, sparky
* type: scenario
* browsers: Android: Fennec 64.0.2, GeckoView Example, Fenix, and Reference Browser
* measuring: Power consumption for idle Android browsers, with about:blank loaded and app foregrounded, over a 20-minute duration

raptor-scn-power-idle-bg
^^^^^^^^^^^^^^^^^^^^^^^^

* contact: stephend, sparky
* type: scenario
* browsers: Android: Fennec 64.0.2, GeckoView Example, Fenix, and Reference Browser
* measuring: Power consumption for idle Android browsers, with about:blank loaded and app backgrounded, over a 10-minute duration

Debugging Desktop Product Failures
**********************************

As of now, there is no easy way to do this. Raptor was not built for debugging functional failures. Hitting these in Raptor is indicative that we lack functional test coverage so regression tests should be added for those failures after they are fixed.

To debug a functional failure in Raptor you can follow these steps:

#. If bug 1653617 has not landed yet, apply the patch.
#. Add the --verbose flag to the extra-options list `here <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/raptor.yml#98-101>`__.
#. If the --setenv doesn't exist yet (`bug 1494669 <https://bugzilla.mozilla.org/show_bug.cgi?id=1494669>`_), then add your MOZ_LOG environment variables to give you additional logging `here <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/webextension/desktop.py#42>`_.
#. If the flag does exist, then you can add the MOZ_LOG variables to the `raptor.yml <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/raptor.yml>`_ configuration file.
#. Push to try if you can't reproduce the failure locally.

You can follow `bug 1655554 <https://bugzilla.mozilla.org/show_bug.cgi?id=1655554>`_ as we work on improving this workflow.

In some cases, you might not be able to get logging for what you are debugging (browser console logging for instance). In this case, you should make your own debug prints with printf or something along those lines (`see :agi's debugging work for an example <https://matrix.to/#/!LfXZSWEroPFPMQcYmw:mozilla.org/$r_azj7OipkgDzQ75SCns2QIayp4260PIMHLWLApJJNg?via=mozilla.org&via=matrix.org&via=rduce.org>`_).

Debugging the Raptor Web Extension
**********************************

When developing on Raptor and debugging, there's often a need to look at the output coming from the `Raptor Web Extension <https://searchfox.org/mozilla-central/source/testing/raptor/webext/raptor>`_. Here are some pointers to help.

Raptor Debug Mode
=================

The easiest way to debug the Raptor web extension is to run the Raptor test locally and invoke debug mode, i.e. for Firefox:

::

  ./mach raptor --test raptor-tp6-amazon-firefox --debug-mode

Or on Chrome, for example:

::

  ./mach raptor --test raptor-tp6-amazon-chrome --app=chrome --binary="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --debug-mode

Running Raptor with debug mode will:

* Automatically set the number of test page-cycles to 2 maximum
* Reduce the 30 second post-browser startup delay from 30 seconds to 3 seconds
* On Firefox, the devtools browser console will automatically open, where you can view all of the console log messages generated by the Raptor web extension
* On Chrome, the devtools console will automatically open
* The browser will remain open after the Raptor test has finished; you will be prompted in the terminal to manually shutdown the browser when you're finished debugging.

Manual Debugging on Firefox Desktop
===================================

The main Raptor runner is '`runner.js <https://searchfox.org/mozilla-central/source/testing/raptor/webext/raptor/runner.js>`_' which is inside the web extension. The code that actually captures the performance measures is in the web extension content code '`measure.js <https://searchfox.org/mozilla-central/source/testing/raptor/webext/raptor/measure.js>`_'.

In order to retrieve the console.log() output from the Raptor runner, do the following:

#. Invoke Raptor locally via ``./mach raptor``
#. During the 30 second Raptor pause which happens right after Firefox has started up, in the ALREADY OPEN current tab, type "about:debugging" for the URL.
#. On the debugging page that appears, make sure "Add-ons" is selected on the left (default).
#. Turn ON the "Enable add-on debugging" check-box
#. Then scroll down the page until you see the Raptor web extension in the list of currently-loaded add-ons. Under "Raptor" click the blue "Debug" link.
#. A new window will open in a minute, and click the "console" tab

To retrieve the console.log() output from the Raptor content 'measure.js' code:

#. As soon as Raptor opens the new test tab (and the test starts running / or the page starts loading), in Firefox just choose "Tools => Web Developer => Web Console", and select the "console' tab.

Raptor automatically closes the test tab and the entire browser after test completion; which will close any open debug consoles. In order to have more time to review the console logs, Raptor can be temporarily hacked locally in order to prevent the test tab and browser from being closed. Currently this must be done manually, as follows:

#. In the Raptor web extension runner, comment out the line that closes the test tab in the test clean-up. That line of `code is here <https://searchfox.org/mozilla-central/rev/3c85ea2f8700ab17e38b82d77cd44644b4dae703/testing/raptor/webext/raptor/runner.js#357>`_.
#. Add a return statement at the top of the Raptor control server method that shuts-down the browser, the browser shut-down `method is here <https://searchfox.org/mozilla-central/rev/924e3d96d81a40d2f0eec1db5f74fc6594337128/testing/raptor/raptor/control_server.py#120>`_.

For **benchmark type tests** (i.e. speedometer, motionmark, etc.) Raptor doesn't inject 'measure.js' into the test page content; instead it injects '`benchmark-relay.js <https://searchfox.org/mozilla-central/source/testing/raptor/webext/raptor/benchmark-relay.js>`_' into the benchmark test content. Benchmark-relay is as it sounds; it basically relays the test results coming from the benchmark test, to the Raptor web extension runner. Viewing the console.log() output from benchmark-relay is done the same was as noted for the 'measure.js' content above.

Note, `Bug 1470450 <https://bugzilla.mozilla.org/show_bug.cgi?id=1470450>`_ is on file to add a debug mode to Raptor that will automatically grab the web extension console output and dump it to the terminal (if possible) that will make debugging much easier.

Debugging TP6 and Killing the Mitmproxy Server
==============================================

Regarding debugging Raptor pageload tests that use Mitmproxy (i.e. tp6, gdocs). If Raptor doesn't finish naturally and doesn't stop the Mitmproxy tool, the next time you attempt to run Raptor it might fail out with this error:

::

    INFO -  Error starting proxy server: OSError(48, 'Address already in use')
    NFO -  raptor-mitmproxy Aborting: mitmproxy playback process failed to start, poll returned: 1

That just means the Mitmproxy server was already running before so it couldn't startup. In this case, you need to kill the Mitmproxy server processes, i.e:

::

    mozilla-unified rwood$ ps -ax | grep mitm
    5439 ttys000    0:00.09 /Users/rwood/mozilla-unified/obj-x86_64-apple-darwin17.7.0/testing/raptor/mitmdump -k -q -s /Users/rwood/mozilla-unified/testing/raptor/raptor/playback/alternate-server-replay.py /Users/rwood/mozilla-unified/obj-x86_64-apple-darwin17.7.0/testing/raptor/amazon.mp
    440 ttys000    0:01.64 /Users/rwood/mozilla-unified/obj-x86_64-apple-darwin17.7.0/testing/raptor/mitmdump -k -q -s /Users/rwood/mozilla-unified/testing/raptor/raptor/playback/alternate-server-replay.py /Users/rwood/mozilla-unified/obj-x86_64-apple-darwin17.7.0/testing/raptor/amazon.mp
    5509 ttys000    0:00.01 grep mitm

Then just kill the first mitm process in the list and that's sufficient:

::

    mozilla-unified rwood$ kill 5439

Now when you run Raptor again, the Mitmproxy server will be able to start.

Manual Debugging on Firefox Android
===================================

Be sure to read the above section first on how to debug the Raptor web extension when running on Firefox Desktop.

When running Raptor tests on Firefox on Android (i.e. geckoview), to see the console.log() output from the Raptor web extension, do the following:

#. With your android device (i.e. Google Pixel 2) all set up and connected to USB, invoke the Raptor test normally via ``./mach raptor``
#. Start up a local copy of the Firefox Nightly Desktop browser
#. In Firefox Desktop choose "Tools => Web Developer => WebIDE"
#. In the Firefox WebIDE dialog that appears, look under "USB Devices" listed on the top right. If your device is not there, there may be a link to install remote device tools - if that link appears click it and let that install.
#. Under "USB Devices" on the top right your android device should be listed (i.e. "Firefox Custom on Android Pixel 2" - click on your device.
#. The debugger opens. On the left side click on "Main Process", and click the "console" tab below - and the Raptor runner output will be included there.
#. On the left side under "Tabs" you'll also see an option for the active tab/page; select that and the Raptor content console.log() output should be included there.

Also note: When debugging Raptor on Android, the 'adb logcat' is very useful. More specifically for 'geckoview', the output (including for Raptor) is prefixed with "GeckoConsole" - so this command is very handy:

::

  adb logcat | grep GeckoConsole

Manual Debugging on Google Chrome
=================================

Same as on Firefox desktop above, but use the Google Chrome console: View ==> Developer ==> Developer Tools.

Raptor on Mobile projects (Fenix, Reference-Browser)
****************************************************

Add new tests
=============

For mobile projects, Raptor tests are on the following repositories:

**Fenix**:

- Repository: `Github <https://github.com/mozilla-mobile/fenix/>`__
- Tests results: `Treeherder view <https://treeherder.mozilla.org/#/jobs?repo=fenix>`__
- Schedule: Every 24 hours `Taskcluster force hook <https://tools.taskcluster.net/hooks/project-releng/cron-task-mozilla-mobile-fenix%2Fraptor>`_

**Reference-Browser**:

- Repository: `Github <https://github.com/mozilla-mobile/reference-browser/>`__
- Tests results: `Treeherder view <https://treeherder.mozilla.org/#/jobs?repo=reference-browser>`__
- Schedule: On each push

Tests are now defined in a similar fashion compared to what exists in mozilla-central. Task definitions are expressed in Yaml:

* https://github.com/mozilla-mobile/fenix/blob/1c9c5317eb33d92dde3293dfe6a857c279a7ab12/taskcluster/ci/raptor/kind.yml
* https://github.com/mozilla-mobile/reference-browser/blob/4560a83cb559d3d4d06383205a8bb76a44336704/taskcluster/ci/raptor/kind.yml

If you want to test your changes on a PR, before they land, you need to apply a patch like this one: https://github.com/mozilla-mobile/fenix/pull/5565/files. Don't forget to revert it before merging the patch.

On Fenix and Reference-Browser, the raptor revision is tied to the latest nightly of mozilla-central

For more information, please reach out to :jlorenzo or :mhentges in #cia

Code formatting on Raptor
*************************
As Raptor is a Mozilla project we follow the general Python coding style:

* https://firefox-source-docs.mozilla.org/tools/lint/coding-style/coding_style_python.html

`black <https://github.com/psf/black/>`_ is the tool used to reformat the Python code.
