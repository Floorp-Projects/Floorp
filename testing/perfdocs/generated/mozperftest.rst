===========
mozperftest
===========

**mozperftest** can be used to run performance tests.


.. toctree::

   running
   writing
   developing
   vision

The following documents all testing we have for mozperftest.
If the owner does not specify the Usage and Description, it's marked N/A.

browser/base/content/test
-------------------------
Performance tests from the 'browser/base/content/test' folder.

perftest_browser_xhtml_dom.js
=============================

:owner: Browser Front-end team
:name: Dom-size

**Measures the size of the DOM**


netwerk/test/perf
-----------------
Performance tests from the 'network/test/perf' folder.

perftest_http3_cloudflareblog.js
================================

:owner: Network Team
:name: cloudflare

**User-journey live site test for cloudflare blog.**

perftest_http3_controlled.js
============================

:owner: Network Team
:name: controlled
:tags: throttlable

**User-journey live site test for controlled server**

perftest_http3_facebook_scroll.js
=================================

:owner: Network Team
:name: facebook-scroll

**Measures the number of requests per second after a scroll.**

perftest_http3_google_image.js
==============================

:owner: Network Team
:name: g-image

**Measures the number of images per second after a scroll.**

perftest_http3_google_search.js
===============================

:owner: Network Team
:name: g-search

**User-journey live site test for google search**

perftest_http3_lucasquicfetch.js
================================

:owner: Network Team
:name: lq-fetch

**Measures the amount of time it takes to load a set of images.**

perftest_http3_youtube_watch.js
===============================

:owner: Network Team
:name: youtube-noscroll

**Measures quality of the video being played.**

perftest_http3_youtube_watch_scroll.js
======================================

:owner: Network Team
:name: youtube-scroll

**Measures quality of the video being played.**


netwerk/test/unit
-----------------
Performance tests from the 'netwerk/test/unit' folder.

test_http3_perf.js
==================

:owner: Network Team
:name: http3 raw
:tags: network,http3,quic
:Default options:

::

 --perfherder
 --perfherder-metrics name:speed,unit:bps
 --xpcshell-cycles 13
 --verbose
 --try-platform linux, mac

**XPCShell tests that verifies the lib integration against a local server**


testing/performance
-------------------
Performance tests from the 'testing/performance' folder.

perftest_bbc_link.js
====================

:owner: Performance Team
:name: BBC Link

**Measures time to load BBC homepage**

perftest_facebook.js
====================

:owner: Performance Team
:name: Facebook

**Measures time to log in to Facebook**

perftest_jsconf_cold.js
=======================

:owner: Performance Team
:name: JSConf (cold)

**Measures time to load JSConf page (cold)**

perftest_jsconf_warm.js
=======================

:owner: Performance Team
:name: JSConf (warm)

**Measures time to load JSConf page (warm)**

perftest_politico_link.js
=========================

:owner: Performance Team
:name: Politico Link

**Measures time to load Politico homepage**

perftest_android_view.js
========================

:owner: Performance Team
:name: VIEW

**Measures cold process view time**

This test launches the appropriate android app, simulating a opening a link through VIEW intent workflow. The application is launched with the intent action android.intent.action.VIEW loading a trivially simple website. The reported metric is the time from process start to navigationStart, reported as processLaunchToNavStart

perftest_youtube_link.js
========================

:owner: Performance Team
:name: YouTube Link

**Measures time to load YouTube video**

perftest_android_main.js
========================

:owner: Performance Team
:name: main

**Measures the time from process start until the Fenix main activity (HomeActivity) reports Fully Drawn**

This test launches Fenix to its main activity (HomeActivity). The application logs "Fully Drawn" when the activity is drawn. Using the android log transformer we measure the time from process start to this event.

perftest_pageload.js
====================

:owner: Performance Team
:name: pageload

**Measures time to load mozilla page**

perftest_perfstats.js
=====================

:owner: Performance Team
:name: perfstats

**Collect perfstats for the given site**

This test launches browsertime with the perfStats option (will collect low-overhead timings, see Bug 1553254). The test currently runs a short user journey. A selection of popular sites are visited, first as cold pageloads, and then as warm.


If you have any questions, please see this `wiki page <https://wiki.mozilla.org/TestEngineering/Performance#Where_to_find_us>`_.
