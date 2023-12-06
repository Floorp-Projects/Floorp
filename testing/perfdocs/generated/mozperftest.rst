===========
Mozperftest
===========

**Mozperftest** can be used to run performance tests.


.. toctree::

   running
   tools
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


dom/serviceworkers/test/performance
-----------------------------------
Performance tests running through Mochitest for Service Workers

test_caching.html
=================

:owner: DOM LWS
:name: Service Worker Caching
:Default options:

::

 --perfherder
 --perfherder-metrics name:No cache,unit:ms,shouldAlert:True, name:Cached,unit:ms,shouldAlert:True, name:No cache again,unit:ms,shouldAlert:True
 --verbose
 --manifest perftest.toml
 --manifest-flavor plain

**Test service worker caching.**

test_fetch.html
===============

:owner: DOM LWS
:name: Service Worker Fetch
:Default options:

::

 --perfherder
 --perfherder-metrics name:Cold fetch,unit:ms,shouldAlert:True, name:Undisturbed fetch,unit:ms,shouldAlert:True, name:Intercepted fetch,unit:ms,shouldAlert:True, name:Liberated fetch,unit:ms,shouldAlert:True, name:Undisturbed XHR,unit:ms,shouldAlert:True, name:Intercepted XHR,unit:ms,shouldAlert:True, name:Liberated XHR,unit:ms,shouldAlert:True
 --verbose
 --manifest perftest.toml
 --manifest-flavor plain

**Test cold and warm fetches.**

test_registration.html
======================

:owner: DOM LWS
:name: Service Worker Registration
:Default options:

::

 --perfherder
 --perfherder-metrics name:Registration,unit:ms,shouldAlert:True, name:Activation,unit:ms,shouldAlert:True, name:Unregistration,unit:ms,shouldAlert:True
 --verbose
 --manifest perftest.toml
 --manifest-flavor plain

**Test registration, activation, and unregistration.**


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

perftest_youtube_link.js
========================

:owner: Performance Team
:name: YouTube Link

**Measures time to load YouTube video**

perftest_android_startup.js
===========================

:owner: Performance Team
:name: android-startup

**Measures android startup times**

This test consists of 2 main tests, cold main first frame(cmff) and cold view nav start(cvns). cold main first frame is the measurement from when you click the app icon & get duration to first frame from 'am start -W'. cold view nav start is the measurement from when you send a VIEW intent & get duration from logcat: START proc to PageStart.

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

perftest_WPT_chrome_init_file.js
================================

:owner: Performance Testing Team
:name: webpagetest-chrome

**Run webpagetest performance pageload tests on Chrome against Alexa top 50 websites**

This mozperftest gets webpagetest to run pageload tests on Chrome against the 50 most popular websites and provide data. The full list of data returned from webpagetest: firstContentfulPaint, visualComplete90, firstPaint, visualComplete99, visualComplete, SpeedIndex, bytesIn,bytesOut, TTFB, fullyLoadedCPUms, fullyLoadedCPUpct, domElements, domContentLoadedEventStart, domContentLoadedEventEnd, loadEventStart, loadEventEnd

perftest_WPT_firefox_init_file.js
=================================

:owner: Performance Testing Team
:name: webpagetest-firefox

**Run webpagetest performance pageload tests on Firefox against Alexa top 50 websites**

This mozperftest gets webpagetest to run pageload tests on Firefox against the 50 most popular websites and provide data. The full list of data returned from webpagetest: firstContentfulPaint, timeToContentfulPaint, visualComplete90, firstPaint, visualComplete99, visualComplete, SpeedIndex, bytesIn, bytesOut, TTFB, fullyLoadedCPUms, fullyLoadedCPUpct, domElements, domContentLoadedEventStart, domContentLoadedEventEnd, loadEventStart, loadEventEnd


If you have any questions, please see this `wiki page <https://wiki.mozilla.org/TestEngineering/Performance#Where_to_find_us>`_.
