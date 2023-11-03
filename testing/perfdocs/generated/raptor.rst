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


.. toctree::
   :titlesonly:

   browsertime
   debugging
   contributing
   metrics

.. contents::
   :depth: 2
   :local:

Raptor tests
************

The following documents all testing we have for Raptor.

Benchmarks
----------
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI. To update any of these benchmarks, see `Updating Benchmark Tests <browsertime.html#updating-benchmark-tests>`_.


.. dropdown:: ares6
   :class-container: anchor-id-ares6-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
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
      * - **browsertime-benchmark-firefox-ares6**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-chrome-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-chrome-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-ares6**
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
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-chrome-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-firefox-ares6**
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
      * - **browsertime-benchmark-chrome-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-ares6**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-ares6**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: assorted-dom
   :class-container: anchor-id-assorted-dom-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 2000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 60000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/assorted-dom
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **screen capture**: true
   * **test url**: `<http://\<host\>:\<port\>/assorted/driver.html?raptor>`__
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
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-chrome-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-chrome-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-assorted-dom**
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
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-chrome-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-firefox-assorted-dom**
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
      * - **browsertime-benchmark-chrome-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-assorted-dom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-assorted-dom**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: jetstream2
   :class-container: anchor-id-jetstream2-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 4
   * **page timeout**: 2000000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/JetStream2
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **subtest lower is better**: false
   * **subtest unit**: score
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-firefox-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-chrome-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-chrome-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-jetstream2**
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
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-chrome-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-firefox-jetstream2**
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
      * - **browsertime-benchmark-chrome-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-jetstream2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-jetstream2**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: matrix-react-bench
   :class-container: anchor-id-matrix-react-bench-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 30
   * **page timeout**: 2000000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/matrix-react-bench
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/matrix_demo.html>`__
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
      * - **browsertime-benchmark-firefox-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-chrome-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-chrome-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-chrome-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-firefox-matrix-react-bench**
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
      * - **browsertime-benchmark-chrome-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-matrix-react-bench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-matrix-react-bench**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-animometer
   :class-container: anchor-id-motionmark-animometer-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-animometer-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: fenix, chrome-m, geckoview
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-animometer-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-animometer-ramp
   :class-container: anchor-id-motionmark-animometer-ramp-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=30&display=minimal&tiles=big&controller=ramp&frame-rate=60&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-ramp-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-animometer-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-ramp-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-ramp-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: fenix, chrome-m, geckoview
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=30&display=minimal&tiles=big&controller=ramp&frame-rate=60&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-ramp-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-animometer-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-animometer-ramp-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-animometer-ramp-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-animometer-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-animometer-ramp**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-htmlsuite
   :class-container: anchor-id-motionmark-htmlsuite-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose chrome trace**: true
   * **expose gecko profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=HTMLsuite&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-htmlsuite-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: fenix, chrome-m, geckoview
   * **expected**: pass
   * **expose chrome trace**: true
   * **expose gecko profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=15&display=minimal&tiles=big&controller=fixed&frame-rate=30&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=HTMLsuite&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-htmlsuite-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: motionmark-htmlsuite-ramp
   :class-container: anchor-id-motionmark-htmlsuite-ramp-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **expected**: pass
   * **expose chrome trace**: true
   * **expose gecko profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=30&display=minimal&tiles=big&controller=ramp&frame-rate=60&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=HTMLsuite&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-ramp-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-htmlsuite-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-ramp-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-ramp-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: fenix, chrome-m, geckoview
   * **expected**: pass
   * **expose chrome trace**: true
   * **expose gecko profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **test url**: `<http://\<host\>:\<port\>/MotionMark/developer.html?test-interval=30&display=minimal&tiles=big&controller=ramp&frame-rate=60&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=HTMLsuite&raptor=true&oskey={platform}>`__
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
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-ramp-nofis**
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
      * - **browsertime-benchmark-motionmark-chrome-m-motionmark-htmlsuite-ramp-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-fenix-motionmark-htmlsuite-ramp-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-motionmark-geckoview-motionmark-htmlsuite-ramp-nofis**
        - ✅
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
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
      * - **browsertime-benchmark-chrome-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-motionmark-htmlsuite-ramp**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-motionmark-htmlsuite-ramp**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: speedometer
   :class-container: anchor-id-speedometer-b

   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
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
      * - **browsertime-benchmark-speedometer-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-speedometer-mobile-chrome-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-cstm-car-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1300-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅


   **Owner**: SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow, chrome-m, cstm-car-m
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
      * - **browsertime-benchmark-speedometer-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-speedometer-mobile-chrome-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-cstm-car-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1300-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer**
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
      * - **browsertime-benchmark-chrome-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: speedometer3
   :class-container: anchor-id-speedometer3-b

   **Owner**: Performance Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browsertime args**: --browsertime.speedometer_iterations=5
   * **custom data**: true
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **host from parent**: false
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 600000
   * **repository**: https://github.com/WebKit/Speedometer
   * **repository revision**: 36712deb57d45879f56dd2136f8a6ed8a22d964b
   * **submetric summary method**: median
   * **subtest lower is better**: true
   * **subtest name filters**: tests/,s3/
   * **subtest unit**: ms
   * **test script**: speedometer3.js
   * **test summary**: flatten
   * **test url**: `<http://\<host\>:\<port\>/index.html>`__
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
      * - **browsertime-benchmark-speedometer3-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-speedometer3-mobile-chrome-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-cstm-car-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1300-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌


   **Owner**: Performance Team

   * **alert threshold**: 2.0
   * **apps**: fenix, geckoview, refbrow, chrome-m, cstm-car-m
   * **browsertime args**: --browsertime.speedometer_iterations=5
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **host from parent**: false
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 420000
   * **repository**: https://github.com/WebKit/Speedometer
   * **repository revision**: 36712deb57d45879f56dd2136f8a6ed8a22d964b
   * **submetric summary method**: median
   * **subtest lower is better**: true
   * **subtest name filters**: tests/,s3/
   * **subtest unit**: ms
   * **test script**: speedometer3.js
   * **test summary**: flatten
   * **test url**: `<http://\<host\>:\<port\>/index.html>`__
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
      * - **browsertime-benchmark-speedometer3-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-speedometer3-mobile-chrome-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-cstm-car-m-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-speedometer3-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1300-64-shippable-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-speedometer3**
        - ❌
        - ✅
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
      * - **browsertime-benchmark-firefox-speedometer3**
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
      * - **browsertime-benchmark-chrome-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-custom-car-speedometer3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-speedometer3**
        - ✅
        - ✅
        - ✅
        - ❌



.. dropdown:: stylebench
   :class-container: anchor-id-stylebench-b

   **Owner**: :emelio and Layout Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 140000
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
      * - **browsertime-benchmark-firefox-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-chrome-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-chrome-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-stylebench**
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
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-chrome-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-firefox-stylebench**
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
      * - **browsertime-benchmark-chrome-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-stylebench**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-stylebench**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: sunspider
   :class-container: anchor-id-sunspider-b

   **Owner**: :jandem and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
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
      * - **browsertime-benchmark-firefox-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-chrome-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-chrome-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-sunspider**
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
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-chrome-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-firefox-sunspider**
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
      * - **browsertime-benchmark-chrome-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-sunspider**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-sunspider**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: twitch-animation
   :class-container: anchor-id-twitch-animation-b

   **Owner**: :jrmuizel

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2000000
   * **perfstats**: true
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/twitch-animation
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **subtest lower is better**: true
   * **subtest unit**: ms
   * **test url**: `<http://\<host\>:\<port\>/index.html>`__
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
      * - **browsertime-benchmark-firefox-twitch-animation**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-twitch-animation**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
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
      * - **browsertime-benchmark-firefox-twitch-animation**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: unity-webgl
   :class-container: anchor-id-unity-webgl-b

   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: geckoview, refbrow, fenix, chrome-m
   * **expected**: pass
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 1
   * **page timeout**: 420000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/unity-webgl
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-unity-webgl-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-unity-webgl-mobile-chrome-m-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-fenix-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌
      * - **browsertime-benchmark-safari-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌


   **Owner**: :jgilbert and Graphics(gfx) Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 8000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **page cycles**: 5
   * **page timeout**: 420000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/unity-webgl
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-unity-webgl-mobile-fenix-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-geckoview-nofis**
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
      * - **browsertime-benchmark-unity-webgl-mobile-chrome-m-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-fenix-nofis**
        - ✅
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-unity-webgl-mobile-geckoview-nofis**
        - ✅
        - ✅
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
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌
      * - **browsertime-benchmark-safari-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-firefox-unity-webgl**
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
      * - **browsertime-benchmark-chrome-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-unity-webgl**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-unity-webgl**
        - ✅
        - ✅
        - ✅
        - ❌



.. dropdown:: wasm-godot
   :class-container: anchor-id-wasm-godot-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ✅
        - ❌
        - ✅
        - ❌
      * - **browsertime-benchmark-wasm-safari-wasm-godot**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-godot**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-godot**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-godot-baseline
   :class-container: anchor-id-wasm-godot-baseline-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-baseline**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-godot-optimizing
   :class-container: anchor-id-wasm-godot-optimizing-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-godot-optimizing**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc
   :class-container: anchor-id-wasm-misc-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/wasm-misc
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
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
      * - **browsertime-benchmark-wasm-chrome-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-chromium-wasm-misc**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-wasm-firefox-wasm-misc**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc-baseline
   :class-container: anchor-id-wasm-misc-baseline-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **preferences**: {"javascript.options.wasm_baselinejit": true, "javascript.options.wasm_optimizingjit": false}
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/wasm-misc
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-baseline**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: wasm-misc-optimizing
   :class-container: anchor-id-wasm-misc-optimizing-b

   **Owner**: :lth and SpiderMonkey Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 1200000
   * **preferences**: {"javascript.options.wasm_baselinejit": false, "javascript.options.wasm_optimizingjit": true}
   * **repository**: https://github.com/mozilla/perf-automation
   * **repository path**: benchmarks/wasm-misc
   * **repository revision**: 61332db584026b73e37066d717a162825408c36b
   * **test url**: `<http://\<host\>:\<port\>/index.html?raptor>`__
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
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
      * - **browsertime-benchmark-wasm-firefox-wasm-misc-optimizing**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: webaudio
   :class-container: anchor-id-webaudio-b

   **Owner**: :padenot and Media Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 4000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 5
   * **page timeout**: 360000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
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
      * - **browsertime-benchmark-firefox-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-chrome-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-chrome-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-benchmark-safari-webaudio**
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
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-chrome-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-firefox-webaudio**
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
      * - **browsertime-benchmark-chrome-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-chromium-webaudio**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-benchmark-firefox-webaudio**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: youtube-playback
   :class-container: anchor-id-youtube-playback-b

   **Owner**: PerfTest Team

   * **alert on**: H264.1080p30@1X_dropped_frames, H264.1080p60@1X_dropped_frames, H264.1440p30@1X_dropped_frames, H264.144p15@1X_dropped_frames, H264.2160p30@1X_dropped_frames, H264.240p30@1X_dropped_frames, H264.360p30@1X_dropped_frames, H264.480p30@1X_dropped_frames, H264.720p30@1X_dropped_frames, H264.720p60@1X_dropped_frames, VP9.1080p30@1X_dropped_frames, VP9.1080p60@1X_dropped_frames, VP9.1440p30@1X_dropped_frames, VP9.1440p60@1X_dropped_frames, VP9.144p30@1X_dropped_frames, VP9.2160p30@1X_dropped_frames, VP9.2160p60@1X_dropped_frames, VP9.240p30@1X_dropped_frames, VP9.360p30@1X_dropped_frames, VP9.480p30@1X_dropped_frames, VP9.720p30@1X_dropped_frames, VP9.720p60@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix,refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<http://yttest.prod.mozaws.net/2019/main.html?test_type=playbackperf-test&raptor=true&command=run&exclude=1,2&muted=true>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true

.. dropdown:: youtube-playback-av1-sfr
   :class-container: anchor-id-youtube-playback-av1-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
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
      * - **browsertime-firefox-youtube-playback-av1-sfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-h264-1080p30
   :class-container: anchor-id-youtube-playback-h264-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-h264-1080p60
   :class-container: anchor-id-youtube-playback-h264-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-h264-full-1080p30
   :class-container: anchor-id-youtube-playback-h264-full-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-h264-test&tests=18&raptor=true&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-h264-full-1080p60
   :class-container: anchor-id-youtube-playback-h264-full-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=46&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-h264-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-h264-sfr
   :class-container: anchor-id-youtube-playback-h264-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
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
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-nofis**
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
      * - **browsertime-mobile-fenix-youtube-playback-h264-sfr-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-h264-sfr-nofis**
        - ✅
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-h264-sfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-hfr
   :class-container: anchor-id-youtube-playback-hfr-b

   **Owner**: PerfTest Team

   * **alert on**: H2641080p60fps@1X_dropped_frames, H264720p60fps@1X_dropped_frames
   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
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
      * - **browsertime-mobile-fenix-youtube-playback-hfr-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-nofis**
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
      * - **browsertime-mobile-fenix-youtube-playback-hfr-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-mobile-geckoview-youtube-playback-hfr-nofis**
        - ✅
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
      * - **browsertime-firefox-youtube-playback-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
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
      * - **browsertime-firefox-youtube-playback-hfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-v9-1080p30
   :class-container: anchor-id-youtube-playback-v9-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-v9-1080p60
   :class-container: anchor-id-youtube-playback-v9-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-v9-full-1080p30
   :class-container: anchor-id-youtube-playback-v9-full-1080p30-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&tests=18&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p30**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-v9-full-1080p60
   :class-container: anchor-id-youtube-playback-v9-full-1080p60-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **expose browser profiler**: true
   * **gather cpuTime**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 20
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "full-screen-api.allow-trusted-requests-only": false, "full-screen-api.warning.timeout": 0}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-hfr-test&raptor=true&tests=14&muted=true&command=run&fullscreen=true&exclude=1,2>`__
   * **type**: benchmark
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
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
      * - **browsertime-power-firefox-youtube-playback-v9-full-1080p60**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-playback-vp9-sfr
   :class-container: anchor-id-youtube-playback-vp9-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false}
   * **subtest lower is better**: true
   * **subtest unit**: score
   * **test url**: `<https://yttest.prod.mozaws.net/2020/main.html?test_type=playbackperf-sfr-vp9-test&raptor=true&exclude=1,2&muted=true&command=run>`__
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-vp9-sfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-h264-sfr
   :class-container: anchor-id-youtube-playback-widevine-h264-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-h264-sfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-hfr
   :class-container: anchor-id-youtube-playback-widevine-hfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-hfr**
        - ✅
        - ❌
        - ✅
        - ❌



.. dropdown:: youtube-playback-widevine-vp9-sfr
   :class-container: anchor-id-youtube-playback-widevine-vp9-sfr-b

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, geckoview, fenix, refbrow, chrome
   * **expected**: pass
   * **expose browser profiler**: true
   * **gecko profile entries**: 50000000
   * **gecko profile interval**: 1000
   * **gecko profile threads**: MediaPlayback
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 2700000
   * **preferences**: {"media.autoplay.default": 0, "media.autoplay.ask-permission": false, "media.autoplay.blocking_policy": 0, "media.allowed-to-play.enabled": true, "media.block-autoplay-until-in-foreground": false, "media.eme.enabled": true, "media.gmp-manager.updateEnabled": true, "media.eme.require-app-approval": false}
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
        - ✅
        - ❌
        - ✅
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
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
      * - **browsertime-firefox-youtube-playback-widevine-vp9-sfr**
        - ✅
        - ❌
        - ✅
        - ❌




Custom
------
Browsertime tests that use a custom pageload test script. These use the pageload type, but may have other intentions.

.. dropdown:: browsertime
   :class-container: anchor-id-browsertime-c

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
   * **playback version**: 8.1.1
   * **test script**: None
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true

.. dropdown:: connect
   :class-container: anchor-id-connect-c

   **Owner**: Network Team

   * **alert change type**: absolute
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange
   * **alert threshold**: 10.0
   * **apps**: firefox
   * **browser cycles**: 3
   * **cold**: true
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 60000
   * **test script**: speculative-connect.js
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
        - ❌
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speculative-firefox-connect**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
        - ❌
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
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
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
      * - **browsertime-speculative-firefox-connect**
        - ❌
        - ✅
        - ❌
        - ❌



.. dropdown:: constant-regression
   :class-container: anchor-id-constant-regression-c

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, geckoview, fenix
   * **browser cycles**: 1
   * **browsertime args**: --browsertime.constant_value=1500
   * **custom data**: true
   * **expected**: pass
   * **fore window**: 1
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **max back window**: 3
   * **min back window**: 2
   * **output timeout**: 2000000
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest unit**: ms
   * **test script**: constant_regression_test.js
   * **test summary**: flatten
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: score
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-regression-tests-firefox-constant-regression**
        - ❌
        - ✅
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
      * - **browsertime-regression-tests-firefox-constant-regression**
        - ❌
        - ✅
        - ❌
        - ❌



.. dropdown:: grandprix
   :class-container: anchor-id-grandprix-c

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **browser cycles**: 1
   * **browsertime args**: --browsertime.grandprix_iterations=25
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **output timeout**: 2000000
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest name filters**: tests/,iterations/
   * **subtest unit**: ms
   * **test script**: grandprix.js
   * **test summary**: flatten
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: score
   * **use live sites**: true

.. dropdown:: process-switch
   :class-container: anchor-id-process-switch-c

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

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
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
      * - **browsertime-custom-firefox-process-switch**
        - ✅
        - ❌
        - ❌
        - ❌



.. dropdown:: sample-python-support
   :class-container: anchor-id-sample-python-support-c

   **Owner**: PerfTest Team

   * **alert threshold**: 2.0
   * **apps**: firefox
   * **browser cycles**: 1
   * **browsertime args**: --browsertime.constant_value={replace-with-constant-value}
   * **custom data**: true
   * **expected**: pass
   * **fore window**: 1
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **max back window**: 3
   * **min back window**: 2
   * **output timeout**: 2000000
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest unit**: ms
   * **support class**: sample_python_support.py
   * **test script**: constant_regression_test.js
   * **test summary**: flatten
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: score
   * **use live sites**: true

.. dropdown:: throttled
   :class-container: anchor-id-throttled-c

   **Owner**: Network Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **browser cycles**: 10
   * **cold**: true
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 1
   * **page timeout**: 60000
   * **test script**: throttled_pageload.js
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-throttled-firefox-throttled**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
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
      * - **browsertime-throttled-firefox-throttled**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: upload
   :class-container: anchor-id-upload-c

   **Owner**: Network Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **browser cycles**: 1
   * **browsertime args**: --browsertime.upload_iterations=10 --firefox.preference=network.http.http3.enable:false --chrome.args disable-quic
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **output timeout**: 2000000
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: false
   * **subtest name filters**: tests/,iterations/
   * **subtest unit**: mbps
   * **test script**: upload.js
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: mbps
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-chrome-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-chrome-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-upload-safari-upload**
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
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-chrome-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-firefox-upload**
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
      * - **browsertime-upload-chrome-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: upload-h3
   :class-container: anchor-id-upload-h3-c

   **Owner**: Network Team

   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
   * **browser cycles**: 1
   * **browsertime args**: --browsertime.upload_iterations=10 --firefox.preference=network.http.http3.enable:true
   * **custom data**: true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: false
   * **output timeout**: 2000000
   * **page cycles**: 1
   * **page timeout**: 1800000
   * **subtest lower is better**: false
   * **subtest name filters**: tests/,iterations/
   * **subtest unit**: mbps
   * **test script**: upload.js
   * **test url**: `<None>`__
   * **type**: pageload
   * **unit**: mbps
   * **use live sites**: true
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-chrome-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload-h3**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-chrome-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload-h3**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-upload-safari-upload-h3**
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
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-chrome-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-firefox-upload-h3**
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
      * - **browsertime-upload-chrome-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-chromium-upload-h3**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-upload-firefox-upload-h3**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: welcome
   :class-container: anchor-id-welcome-c

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
   * **playback version**: 8.1.1
   * **test script**: welcome.js
   * **test url**: `<about:welcome>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:

   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
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
      * - **browsertime-first-install-firefox-welcome**
        - ✅
        - ✅
        - ❌
        - ❌




Desktop
-------
Tests for page-load performance. The links direct to the actual websites that are being tested.

.. dropdown:: amazon
   :class-container: anchor-id-amazon-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-amazon.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6m-chrome-m-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing-search
   :class-container: anchor-id-bing-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
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
      * - **browsertime-tp6-bytecode-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-bing-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-bing-search**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: buzzfeed
   :class-container: anchor-id-buzzfeed-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-buzzfeed.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-chrome-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-chrome-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-chrome-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-firefox-buzzfeed**
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
      * - **browsertime-tp6-chrome-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-buzzfeed**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-buzzfeed**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: cnn
   :class-container: anchor-id-cnn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-cnn.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6m-chrome-m-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay
   :class-container: anchor-id-ebay-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm8-linux-firefox-ebay.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-ebay**
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
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
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
      * - **browsertime-tp6-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-ebay**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-ebay**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-ebay**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: espn
   :class-container: anchor-id-espn-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-espn.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-nofis**
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
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: expedia
   :class-container: anchor-id-expedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-expedia.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://groups.expedia.com/Group-Rate/?locale=en_US&ol=1>`__
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
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
      * - **browsertime-tp6m-live-fenix-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-nofis**
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
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: facebook
   :class-container: anchor-id-facebook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-facebook.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: fandom
   :class-container: anchor-id-fandom-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
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
      * - **browsertime-tp6-bytecode-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-fandom**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-fandom**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-fandom**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-docs
   :class-container: anchor-id-google-docs-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-google-docs**
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
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
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
      * - **browsertime-tp6-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-docs**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-docs**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-mail
   :class-container: anchor-id-google-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-google-mail**
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
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
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
      * - **browsertime-tp6-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-mail**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-search
   :class-container: anchor-id-google-search-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-google-search**
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
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
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
      * - **browsertime-tp6-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-google-search**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-search**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-slides
   :class-container: anchor-id-google-slides-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
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
      * - **browsertime-tp6-bytecode-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-google-slides**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-google-slides**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb
   :class-container: anchor-id-imdb-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-imdb.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://www.imdb.com/title/tt0084967/episodes/?ref_=tt_ov_epl>`__
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
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
      * - **browsertime-tp6m-fenix-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb**
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
      * - **browsertime-tp6m-chrome-m-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imgur
   :class-container: anchor-id-imgur-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm8-linux-firefox-imgur.manifest
   * **playback version**: 8.1.1
   * **preferences**: {"media.autoplay.default": 5, "media.autoplay.ask-permission": true, "media.autoplay.blocking_policy": 1, "media.allowed-to-play.enabled": false, "media.block-autoplay-until-in-foreground": true}
   * **secondary url**: `<https://imgur.com/gallery/rCXZUil>`__
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
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-chrome-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-chrome-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-imgur**
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
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-chrome-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
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
      * - **browsertime-tp6-chrome-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imgur**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chromium-imgur**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imgur**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: instagram
   :class-container: anchor-id-instagram-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-instagram.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://www.instagram.com/nobelprize_org/>`__
   * **test url**: `<https://www.instagram.com/>`__
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
      * - **browsertime-tp6m-fenix-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram**
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
      * - **browsertime-tp6m-chrome-m-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: linkedin
   :class-container: anchor-id-linkedin-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-linkedin.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-linkedin**
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
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
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
      * - **browsertime-tp6-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-linkedin**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-linkedin**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: microsoft
   :class-container: anchor-id-microsoft-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-microsoft.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-microsoft**
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
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
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
      * - **browsertime-tp6-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-microsoft**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-microsoft**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: netflix
   :class-container: anchor-id-netflix-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-netflix.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
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
      * - **browsertime-tp6-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-netflix**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-netflix**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-netflix**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: nytimes
   :class-container: anchor-id-nytimes-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-nytimes.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://www.nytimes.com/section/opinion/columnists>`__
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
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
      * - **browsertime-tp6m-live-fenix-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-nofis**
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
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: office
   :class-container: anchor-id-office-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-office.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-office**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-chrome-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-chrome-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-chrome-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-firefox-office**
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
      * - **browsertime-tp6-chrome-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-office**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-office**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: outlook
   :class-container: anchor-id-outlook-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
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
      * - **browsertime-tp6-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-outlook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-outlook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-outlook**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: paypal
   :class-container: anchor-id-paypal-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-paypal**
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
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
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
      * - **browsertime-tp6-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-paypal**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-paypal**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-paypal**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: pinterest
   :class-container: anchor-id-pinterest-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-pinterest.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-pinterest**
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
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
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
      * - **browsertime-tp6-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-pinterest**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-pinterest**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit
   :class-container: anchor-id-reddit-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-reddit.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://www.reddit.com/r/technology/>`__
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__
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
      * - **browsertime-tp6m-fenix-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit**
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
      * - **browsertime-tp6m-chrome-m-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: tumblr
   :class-container: anchor-id-tumblr-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm6-linux-firefox-tumblr.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-tumblr**
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
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
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
      * - **browsertime-tp6-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-tumblr**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-tumblr**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: twitch
   :class-container: anchor-id-twitch-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-twitch.manifest
   * **playback version**: 8.1.1
   * **preferences**: {"media.autoplay.default": 5, "media.autoplay.ask-permission": true, "media.autoplay.blocking_policy": 1, "media.allowed-to-play.enabled": false, "media.block-autoplay-until-in-foreground": true}
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
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-twitch**
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
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
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
      * - **browsertime-tp6-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-twitch**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitch**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitch**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: twitter
   :class-container: anchor-id-twitter-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
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
      * - **browsertime-tp6-bytecode-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-twitter**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-twitter**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-twitter**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikia
   :class-container: anchor-id-wikia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-wikia.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-firefox-wikia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-chrome-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-chrome-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-chrome-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-firefox-wikia**
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
      * - **browsertime-tp6-chrome-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-wikia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-wikia**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: wikipedia
   :class-container: anchor-id-wikipedia-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-wikipedia.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://en.wikipedia.org/wiki/Joe_Biden>`__
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
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
      * - **browsertime-tp6m-fenix-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia**
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
      * - **browsertime-tp6m-chrome-m-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: yahoo-mail
   :class-container: anchor-id-yahoo-mail-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
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
      * - **browsertime-tp6-bytecode-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-yahoo-mail**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-yahoo-mail**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube
   :class-container: anchor-id-youtube-d

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari, custom-car
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-youtube.manifest
   * **playback version**: 8.1.1
   * **secondary url**: `<https://www.youtube.com/watch?v=JrdEMERq8MA>`__
   * **test url**: `<https://www.youtube.com>`__
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
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6m-chrome-m-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌




Interactive
-----------
Browsertime tests that interact with the webpage. Includes responsiveness tests as they make use of this support for navigation. These form of tests allow the specification of browsertime commands through the test manifest.

.. dropdown:: cnn-nav
   :class-container: anchor-id-cnn-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
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
   * **playback version**: 8.1.1
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
      * - **browsertime-responsiveness-firefox-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-chrome-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-chrome-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-chrome-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-firefox-cnn-nav**
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
      * - **browsertime-responsiveness-chrome-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-cnn-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-cnn-nav**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: facebook-nav
   :class-container: anchor-id-facebook-nav-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
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
   * **playback version**: 8.1.1
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
      * - **browsertime-responsiveness-firefox-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-chrome-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-chrome-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-chrome-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-firefox-facebook-nav**
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
      * - **browsertime-responsiveness-chrome-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-facebook-nav**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-facebook-nav**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-ama
   :class-container: anchor-id-reddit-billgates-ama-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
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
   * **playback version**: 8.1.1
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-ama**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-ama**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-post-1
   :class-container: anchor-id-reddit-billgates-post-1-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
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
   * **playback version**: 8.1.1
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-1**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-1**
        - ✅
        - ✅
        - ❌
        - ❌



.. dropdown:: reddit-billgates-post-2
   :class-container: anchor-id-reddit-billgates-post-2-i

   **Owner**: PerfTest Team

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime, ContentfulSpeedIndex, PerceptualSpeedIndex, SpeedIndex, FirstVisualChange, LastVisualChange
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium, safari
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
   * **playback version**: 8.1.1
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
        - ✅
        - ✅
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
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
      * - **browsertime-responsiveness-chrome-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-chromium-reddit-billgates-post-2**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-responsiveness-firefox-reddit-billgates-post-2**
        - ✅
        - ✅
        - ❌
        - ❌




Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms.

.. dropdown:: booking-sf
   :class-container: anchor-id-booking-sf-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-booking-sf-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-booking-sf-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-sf-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-sf-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn
   :class-container: anchor-id-cnn-l

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
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6m-chrome-m-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn-ampstories
   :class-container: anchor-id-cnn-ampstories-l

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
      * - **browsertime-tp6m-fenix-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories**
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
      * - **browsertime-tp6m-chrome-m-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-nofis**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: discord
   :class-container: anchor-id-discord-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-discord-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-discord-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-discord-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-discord-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: expedia
   :class-container: anchor-id-expedia-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-expedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-expedia-nofis**
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
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-firefox-expedia**
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
      * - **browsertime-tp6-chrome-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-expedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-expedia**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: fashionbeans
   :class-container: anchor-id-fashionbeans-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-fashionbeans-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-fashionbeans-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-fashionbeans-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-fashionbeans-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-accounts
   :class-container: anchor-id-google-accounts-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-google-accounts-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-google-accounts-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-accounts-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-accounts-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb-firefox
   :class-container: anchor-id-imdb-firefox-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-imdb-firefox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-imdb-firefox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-firefox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-firefox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: medium-article
   :class-container: anchor-id-medium-article-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-medium-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-medium-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-medium-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-medium-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: nytimes
   :class-container: anchor-id-nytimes-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-nytimes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-nytimes-nofis**
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
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-firefox-nytimes**
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
      * - **browsertime-tp6-chrome-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-nytimes**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-nytimes**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: people-article
   :class-container: anchor-id-people-article-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-people-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-people-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-people-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-people-article-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit-thread
   :class-container: anchor-id-reddit-thread-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-reddit-thread-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-reddit-thread-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-thread-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-thread-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: rumble-fox
   :class-container: anchor-id-rumble-fox-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-rumble-fox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-rumble-fox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-rumble-fox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-rumble-fox-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: stackoverflow-question
   :class-container: anchor-id-stackoverflow-question-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-question-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-question-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-question-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: urbandictionary-define
   :class-container: anchor-id-urbandictionary-define-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-urbandictionary-define-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-urbandictionary-define-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-urbandictionary-define-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikia-marvel
   :class-container: anchor-id-wikia-marvel-l

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

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-live-fenix-wikia-marvel-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-nofis**
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
      * - **browsertime-tp6m-live-chrome-m-wikia-marvel-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikia-marvel-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikia-marvel-nofis**
        - ❌
        - ❌
        - ❌
        - ❌




Mobile
------
Page-load performance test suite on Android. The links direct to the actual websites that are being tested.

.. dropdown:: allrecipes
   :class-container: anchor-id-allrecipes-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-allrecipes-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-allrecipes-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: amazon
   :class-container: anchor-id-amazon-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6m-chrome-m-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: amazon-search
   :class-container: anchor-id-amazon-search-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-amazon-search-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing
   :class-container: anchor-id-bing-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-bing**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing**
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
      * - **browsertime-tp6m-chrome-m-bing**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: bing-search-restaurants
   :class-container: anchor-id-bing-search-restaurants-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants**
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
      * - **browsertime-tp6m-chrome-m-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-bing-search-restaurants-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-bing-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-bing-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: booking
   :class-container: anchor-id-booking-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-booking**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking**
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
      * - **browsertime-tp6m-chrome-m-booking**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-booking-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-booking-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-booking**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn
   :class-container: anchor-id-cnn-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6m-chrome-m-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
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
      * - **browsertime-tp6-bytecode-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-cnn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-cnn**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-sheriffed-firefox-cnn**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: cnn-ampstories
   :class-container: anchor-id-cnn-ampstories-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories**
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
      * - **browsertime-tp6m-chrome-m-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-cnn-ampstories-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-cnn-ampstories-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-cnn-ampstories-nofis**
        - ❌
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-cnn-ampstories**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: dailymail
   :class-container: anchor-id-dailymail-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail**
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
      * - **browsertime-tp6m-chrome-m-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-dailymail-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-dailymail-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-dailymail**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay-kleinanzeigen
   :class-container: anchor-id-ebay-kleinanzeigen-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen**
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
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: ebay-kleinanzeigen-search
   :class-container: anchor-id-ebay-kleinanzeigen-search-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search**
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
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-ebay-kleinanzeigen-search**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: espn
   :class-container: anchor-id-espn-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-espn-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-espn-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-espn-nofis**
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
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-firefox-espn**
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
      * - **browsertime-tp6-chrome-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-espn**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-espn**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: facebook
   :class-container: anchor-id-facebook-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: facebook-cristiano
   :class-container: anchor-id-facebook-cristiano-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-fenix-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano**
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
      * - **browsertime-tp6m-chrome-m-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-facebook-cristiano-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-cristiano-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-facebook-cristiano**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google
   :class-container: anchor-id-google-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-maps
   :class-container: anchor-id-google-maps-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://www.google.com/maps?force=pwa>`__
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
      * - **browsertime-tp6m-fenix-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps**
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
      * - **browsertime-tp6m-chrome-m-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-maps-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-maps-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-maps**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google-search-restaurants
   :class-container: anchor-id-google-search-restaurants-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://www.google.com/search?q=restaurants+near+me>`__
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
      * - **browsertime-tp6m-fenix-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants**
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
      * - **browsertime-tp6m-chrome-m-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-google-search-restaurants-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-search-restaurants-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-google-search-restaurants**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: imdb
   :class-container: anchor-id-imdb-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://m.imdb.com/>`__
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
      * - **browsertime-tp6m-fenix-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb**
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
      * - **browsertime-tp6m-chrome-m-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-imdb-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-imdb-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
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
      * - **browsertime-tp6-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-imdb**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-imdb**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-imdb**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: instagram
   :class-container: anchor-id-instagram-m

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
   * **playback pageset manifest**: mitm8-android-gve-instagram.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://www.instagram.com>`__
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
      * - **browsertime-tp6m-fenix-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram**
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
      * - **browsertime-tp6m-chrome-m-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-instagram-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-instagram-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
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
      * - **browsertime-tp6-bytecode-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-instagram**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-instagram**
        - ✅
        - ✅
        - ✅
        - ✅



.. dropdown:: microsoft-support
   :class-container: anchor-id-microsoft-support-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-microsoft-support-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-microsoft-support-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: reddit
   :class-container: anchor-id-reddit-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://www.reddit.com>`__
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
      * - **browsertime-tp6m-fenix-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit**
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
      * - **browsertime-tp6m-chrome-m-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-reddit-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-reddit-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
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
      * - **browsertime-tp6-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-reddit**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-reddit**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-reddit**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: sina
   :class-container: anchor-id-sina-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://www.sina.com.cn/>`__
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
      * - **browsertime-tp6m-fenix-sina**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina**
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
      * - **browsertime-tp6m-chrome-m-sina**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-sina-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-sina-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-sina-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-sina**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: stackoverflow
   :class-container: anchor-id-stackoverflow-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://stackoverflow.com/>`__
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
      * - **browsertime-tp6m-fenix-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow**
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
      * - **browsertime-tp6m-chrome-m-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-stackoverflow-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-stackoverflow-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-stackoverflow**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: web-de
   :class-container: anchor-id-web-de-m

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
   * **playback pageset manifest**: mitm7-android-gve-p2-web-de.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://web.de/magazine/politik/politologe-glaubt-grossen-koalition-herbst-knallen-33563566>`__
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
      * - **browsertime-tp6m-fenix-web-de**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de**
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
      * - **browsertime-tp6m-chrome-m-web-de**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-web-de-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-web-de-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-web-de**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: wikipedia
   :class-container: anchor-id-wikipedia-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://en.m.wikipedia.org/wiki/Main_Page>`__
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
      * - **browsertime-tp6m-fenix-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia**
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
      * - **browsertime-tp6m-chrome-m-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-wikipedia-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-wikipedia-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
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
      * - **browsertime-tp6-bytecode-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-wikipedia**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-wikipedia**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube
   :class-container: anchor-id-youtube-m

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
   * **playback version**: 8.1.1
   * **test url**: `<https://m.youtube.com>`__
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
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6m-chrome-m-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube-watch
   :class-container: anchor-id-youtube-watch-m

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
   * **playback version**: 8.1.1
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
      * - **browsertime-tp6m-essential-fenix-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-youtube-watch-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-watch-nofis**
        - ❌
        - ❌
        - ❌
        - ❌




Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc.

.. dropdown:: idle
   :class-container: anchor-id-idle-s

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

.. dropdown:: idle-bg
   :class-container: anchor-id-idle-bg-s

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


Unittests
---------
These tests aren't used in standard testing, they are only used in the Raptor unit tests (they are similar to raptor-tp6 tests though).

.. dropdown:: amazon
   :class-container: anchor-id-amazon-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-amazon.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6m-chrome-m-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-amazon-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-amazon-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
        - ✅
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-profiling-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
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
      * - **browsertime-tp6-bytecode-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-essential-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-custom-car-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-essential-firefox-amazon**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-amazon**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-amazon**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: facebook
   :class-container: anchor-id-facebook-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-facebook.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-facebook-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-facebook-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-facebook-nofis**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
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
      * - **browsertime-tp6-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-facebook**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-facebook**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-facebook**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: google
   :class-container: anchor-id-google-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-google.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__
   * **type**: pageload
   * **unit**: ms
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-essential-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-nofis**
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
      * - **browsertime-tp6m-essential-chrome-m-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-essential-geckoview-google-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-google-nofis**
        - ❌
        - ❌
        - ❌
        - ❌



.. dropdown:: youtube
   :class-container: anchor-id-youtube-u

   **Owner**: PerfTest Team

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm7-linux-firefox-youtube.manifest
   * **playback version**: 8.1.1
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **Test Task**:

   .. list-table:: **test-android-hw-a51-11-0-aarch64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6m-chrome-m-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-geckoview-youtube-nofis**
        - ✅
        - ✅
        - ❌
        - ❌
      * - **browsertime-tp6m-live-chrome-m-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-fenix-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-live-geckoview-youtube-nofis**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6m-refbrow-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-linux1804-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-macosx1015-64-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-safari-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌


   .. list-table:: **test-windows10-64-nightlyasrelease-qr/opt**
      :widths: 30 15 15 15 15
      :header-rows: 1

      * - **Test Name**
        - mozilla-central
        - autoland
        - mozilla-release
        - mozilla-beta
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
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
      * - **browsertime-tp6-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-firefox-youtube**
        - ✅
        - ✅
        - ✅
        - ✅
      * - **browsertime-tp6-live-chrome-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-chromium-youtube**
        - ❌
        - ❌
        - ❌
        - ❌
      * - **browsertime-tp6-live-firefox-youtube**
        - ❌
        - ❌
        - ❌
        - ❌




