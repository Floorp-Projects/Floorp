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
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI.

.. dropdown:: ares6
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: assorted-dom
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: jetstream2
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: matrix-react-bench
   :container: + anchor-id-matrix-react-bench-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **fetch task**: matrix-react-bench
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 30
   * **page timeout**: 2000000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/matrix-react-bench/matrix_demo.html>`__
   * **type**: benchmark
   * **unit**: ms
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-animometer
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-htmlsuite
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: raptor-speedometer-geckoview
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

.. dropdown:: raptor-youtube-playback-h264-1080p30-firefox
   :container: + anchor-id-raptor-youtube-playback-h264-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-h264-1080p60-firefox
   :container: + anchor-id-raptor-youtube-playback-h264-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-h264-full-1080p30-firefox
   :container: + anchor-id-raptor-youtube-playback-h264-full-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-h264-full-1080p60-firefox
   :container: + anchor-id-raptor-youtube-playback-h264-full-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-v9-1080p30-firefox
   :container: + anchor-id-raptor-youtube-playback-v9-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-v9-1080p60-firefox
   :container: + anchor-id-raptor-youtube-playback-v9-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-v9-full-1080p30-firefox
   :container: + anchor-id-raptor-youtube-playback-v9-full-1080p30-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: raptor-youtube-playback-v9-full-1080p60-firefox
   :container: + anchor-id-raptor-youtube-playback-v9-full-1080p60-firefox-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: speedometer
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow, chrome-m
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 420000
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/Speedometer/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-speedometer-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: stylebench
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: sunspider
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: unity-webgl
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌


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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-unity-webgl-mobile-chrome-m-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-fenix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-geckoview-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-unity-webgl-mobile-refbrow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ❌



.. dropdown:: wasm-godot
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-godot-baseline
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-godot-optimizing
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-chrome-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc-baseline
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc-optimizing
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: webaudio
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **test url**: `<http://\<host\>:\<port\>/webaudio/index.html?raptor>`__
   * **type**: benchmark
   * **unit**: score
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: youtube-playback
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<http://yttest.prod.mozaws.net/2019/main.html?test_type=playbackperf-test&raptor=true&command=run&exclude=1,2&muted=true>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-av1-sfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-av1-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-av1-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-av1-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-h264-1080p30
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-h264-1080p60
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-h264-full-1080p30
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-h264-full-1080p60
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-h264-sfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-hfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-v9-1080p30
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-v9-1080p60
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-v9-full-1080p30
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-v9-full-1080p60
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-vp9-sfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-vp9-sfr-e10s**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-mobile-fenix-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-refbrow-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-h264-sfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-h264-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-hfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-hfr-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-hfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-vp9-sfr
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
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.autoplay.block-webaudio": false, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-widevine-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr-fis-e10s**
        - ✅
        - ❌
        - ✅
        - ❌




Custom
------
Browsertime tests that use a custom pageload test script. These use the pageload type, but may have other intentions.

.. dropdown:: browsertime
   :container: + anchor-id-browsertime-c

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **browser cycles**: 1
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 1
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: null.manifest
   * **playback version**: 5.1.1
   * **test script**: None
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true

.. dropdown:: process-switch
   :container: + anchor-id-process-switch-c

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **browsertime args**: --pageCompleteWaitTime=1000 --pageCompleteCheckInactivity=true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-custom-firefox-process-switch-fis-e10s**
        - ✅
        - ❌
        - ❌
        - ❌



.. dropdown:: welcome
   :container: + anchor-id-welcome-c

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-first-install-firefox-welcome-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌




Desktop
-------
Tests for page-load performance. The links direct to the actual websites that are being tested.

.. dropdown:: amazon
   :container: + anchor-id-amazon-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-amazon.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.amazon.com/Acer-A515-46-R14K-Quad-Core-Processor-Backlit/dp/B08VKNVDDR/ref=sr_1_3?dchild=1&keywords=laptop&qid=1627047187&sr=8-3>`__
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-profiling-geckoview-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing-search
   :container: + anchor-id-bing-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: buzzfeed
   :container: + anchor-id-buzzfeed-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-buzzfeed.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.buzzfeed.com/quizzes>`__
   * **test url**: `<https://www.buzzfeed.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: cnn
   :container: + anchor-id-cnn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-cnn.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.cnn.com/weather>`__
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay
   :container: + anchor-id-ebay-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-ebay.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.ebay.com/deals>`__
   * **test url**: `<https://www.ebay.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: espn
   :container: + anchor-id-espn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-espn.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.espn.com/nba/draft/news>`__
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: expedia
   :container: + anchor-id-expedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-expedia.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://groups.expedia.com/Group-Rate/?locale=en_US&ol=1>`__
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: facebook
   :container: + anchor-id-facebook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: fandom
   :container: + anchor-id-fandom-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-docs
   :container: + anchor-id-google-docs-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page complete wait time**: 8000
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-google-docs.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://docs.google.com/document/d/1vUnn0ePU-ynArE1OdxyEHXR2G0sl74ja_st_4OOzlgE/preview>`__
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-docs-canvas
   :container: + anchor-id-google-docs-canvas-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs-canvas-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: google-mail
   :container: + anchor-id-google-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-search
   :container: + anchor-id-google-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-slides
   :container: + anchor-id-google-slides-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb
   :container: + anchor-id-imdb-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-imdb.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.imdb.com/title/tt0084967/episodes/?ref_=tt_ov_epl>`__
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imgur
   :container: + anchor-id-imgur-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-imgur.manifest
   * **playback version**: 7.0.4
   * **preferences**: {"media.autoplay.default": 5, "media.autoplay.ask-permission": true, "media.autoplay.blocking_policy": 1, "media.autoplay.block-webaudio": true, "media.allowed-to-play.enabled": false, "media.block-autoplay-until-in-foreground": true}
   * **secondary url**: `<https://imgur.com/gallery/L13Ci>`__
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: instagram
   :container: + anchor-id-instagram-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: linkedin
   :container: + anchor-id-linkedin-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: microsoft
   :container: + anchor-id-microsoft-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-microsoft.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://support.microsoft.com/en-us>`__
   * **test url**: `<https://www.microsoft.com/en-us/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: netflix
   :container: + anchor-id-netflix-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: nytimes
   :container: + anchor-id-nytimes-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-nytimes.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.nytimes.com/section/opinion/columnists>`__
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: office
   :container: + anchor-id-office-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-office.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.office.com/>`__
   * **test url**: `<https://www.office.com/launch/powerpoint?auth=1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: outlook
   :container: + anchor-id-outlook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: paypal
   :container: + anchor-id-paypal-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: pinterest
   :container: + anchor-id-pinterest-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit
   :container: + anchor-id-reddit-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: tumblr
   :container: + anchor-id-tumblr-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: twitch
   :container: + anchor-id-twitch-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-twitch.manifest
   * **playback version**: 7.0.4
   * **preferences**: {"media.autoplay.default": 5, "media.autoplay.ask-permission": true, "media.autoplay.blocking_policy": 1, "media.autoplay.block-webaudio": true, "media.allowed-to-play.enabled": false, "media.block-autoplay-until-in-foreground": true}
   * **secondary url**: `<https://www.twitch.tv/gmashley>`__
   * **test url**: `<https://www.twitch.tv/videos/894226211>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: twitter
   :container: + anchor-id-twitter-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikia
   :container: + anchor-id-wikia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-wikia.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://marvel.fandom.com/wiki/Celestials>`__
   * **test url**: `<https://marvel.fandom.com/wiki/Black_Panther>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: wikipedia
   :container: + anchor-id-wikipedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-wikipedia.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://en.wikipedia.org/wiki/Joe_Biden>`__
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: yahoo-mail
   :container: + anchor-id-yahoo-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube
   :container: + anchor-id-youtube-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-youtube.manifest
   * **playback version**: 7.0.4
   * **secondary url**: `<https://www.youtube.com/watch?v=JrdEMERq8MA>`__
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-profiling-geckoview-youtube-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌




Interactive
-----------
Browsertime tests that interact with the webpage. Includes responsiveness tests as they make use of this support for navigation. These form of tests allow the specification of browsertime commands through the test manifest.

.. dropdown:: cnn-nav
   :container: + anchor-id-cnn-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-cnn-nav.manifest
   * **playback version**: 7.0.4
   * **test cmds**:  ["measure.start", "landing"], ["navigate", "https://www.cnn.com"], ["wait.byTime", 4000], ["measure.stop", ""], ["measure.start", "world"], ["click.byXpathAndWait", "/html/body/div[5]/div/div/header/div/div[1]/div/div[2]/nav/ul/li[2]/a"], ["wait.byTime", 1000], ["measure.stop", ""],
   * **test url**: `<https://www.cnn.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: facebook-nav
   :container: + anchor-id-facebook-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-ama
   :container: + anchor-id-reddit-billgates-ama-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 240000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-windows-firefox-reddit-billgates-ama.manifest
   * **playback version**: 6.0.2
   * **test cmds**:  ["measure.start", "billg-ama"], ["navigate", "https://www.reddit.com/r/IAmA/comments/m8n4vt/im_bill_gates_cochair_of_the_bill_and_melinda/"], ["wait.byTime", 5000], ["measure.stop", ""], ["measure.start", "members"], ["click.byXpathAndWait", "/html/body/div[1]/div/div[2]/div[2]/div/div[3]/div[2]/div/div[1]/div/div[4]/div[1]/div"], ["wait.byTime", 1000], ["measure.stop", ""],
   * **test url**: `<https://www.reddit.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-post-1
   :container: + anchor-id-reddit-billgates-post-1-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-post-2
   :container: + anchor-id-reddit-billgates-post-2-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 10
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **interactive**: true
   * **lower is better**: true
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

   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2-fis-e10s**
        - ✅
        - ✅
        - ❌
        - ❌




Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms.

.. dropdown:: booking-sf
   :container: + anchor-id-booking-sf-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn
   :container: + anchor-id-cnn-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn-ampstories
   :container: + anchor-id-cnn-ampstories-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: discord
   :container: + anchor-id-discord-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: expedia
   :container: + anchor-id-expedia-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: fashionbeans
   :container: + anchor-id-fashionbeans-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-accounts
   :container: + anchor-id-google-accounts-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb-firefox
   :container: + anchor-id-imdb-firefox-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: medium-article
   :container: + anchor-id-medium-article-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: nytimes
   :container: + anchor-id-nytimes-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: people-article
   :container: + anchor-id-people-article-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit-thread
   :container: + anchor-id-reddit-thread-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: rumble-fox
   :container: + anchor-id-rumble-fox-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: stackoverflow-question
   :container: + anchor-id-stackoverflow-question-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: urbandictionary-define
   :container: + anchor-id-urbandictionary-define-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikia-marvel
   :container: + anchor-id-wikia-marvel-l

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-e10s**
        - ❌
        - ❌
        - ❌
        - ❌




Mobile
------
Page-load performance test suite on Android. The links direct to the actual websites that are being tested.

.. dropdown:: allrecipes
   :container: + anchor-id-allrecipes-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: amazon
   :container: + anchor-id-amazon-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-profiling-geckoview-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: amazon-search
   :container: + anchor-id-amazon-search-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing
   :container: + anchor-id-bing-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-bing-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing-search-restaurants
   :container: + anchor-id-bing-search-restaurants-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-bing-search-restaurants-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: booking
   :container: + anchor-id-booking-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-booking-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn
   :container: + anchor-id-cnn-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn-ampstories
   :container: + anchor-id-cnn-ampstories-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: dailymail
   :container: + anchor-id-dailymail-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-dailymail-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay-kleinanzeigen
   :container: + anchor-id-ebay-kleinanzeigen-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-ebay-kleinanzeigen-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay-kleinanzeigen-search
   :container: + anchor-id-ebay-kleinanzeigen-search-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-ebay-kleinanzeigen-search-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: espn
   :container: + anchor-id-espn-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: facebook
   :container: + anchor-id-facebook-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: facebook-cristiano
   :container: + anchor-id-facebook-cristiano-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-a51-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-a51-geckoview-facebook-cristiano-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-a51-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google
   :container: + anchor-id-google-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-maps
   :container: + anchor-id-google-maps-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-search-restaurants
   :container: + anchor-id-google-search-restaurants-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb
   :container: + anchor-id-imdb-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: instagram
   :container: + anchor-id-instagram-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: microsoft-support
   :container: + anchor-id-microsoft-support-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit
   :container: + anchor-id-reddit-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: sina
   :container: + anchor-id-sina-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: stackoverflow
   :container: + anchor-id-stackoverflow-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: web-de
   :container: + anchor-id-web-de-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikipedia
   :container: + anchor-id-wikipedia-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-essential-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube
   :container: + anchor-id-youtube-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-profiling-geckoview-youtube-e10s**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-clang-trunk-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-32-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube-fis-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube-fis-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-watch
   :container: + anchor-id-youtube-watch-m

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-a51-11-0-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-refbrow-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-e10s**
        - ❌
        - ❌
        - ❌
        - ❌




Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc.

.. dropdown:: idle
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: idle-bg
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

   .. list-table:: **test-android-hw-g5-7-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-g5-7-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-android-hw-p2-8-0-arm7-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-fenix-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-geckoview-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-power-refbrow-idle-bg-e10s**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: raptor-scn-power-idle-bg-fenix
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

.. dropdown:: raptor-scn-power-idle-bg-geckoview
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

.. dropdown:: raptor-scn-power-idle-bg-refbrow
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

.. dropdown:: raptor-scn-power-idle-fenix
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

.. dropdown:: raptor-scn-power-idle-geckoview
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

.. dropdown:: raptor-scn-power-idle-refbrow
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

.. dropdown:: raptor-tp6-unittest-amazon-firefox
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

.. dropdown:: raptor-tp6-unittest-facebook-firefox
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

.. dropdown:: raptor-tp6-unittest-google-firefox
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

.. dropdown:: raptor-tp6-unittest-youtube-firefox
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


