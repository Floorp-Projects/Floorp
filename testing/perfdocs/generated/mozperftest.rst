===========
mozperftest
===========

**mozperftest** can be used to run performance tests.


.. toctree::

   running
   writing
   developing

The following documents all testing we have for mozperftest.
If the owner does not specify the Usage and Description, it's marked N/A.

browser/base/content/test
-------------------------
Performance tests from the 'browser/base/content/test' folder.

perftest_browser_xhtml_dom.js
=============================

Measures the size of the DOM

| Owner: Browser Front-end team
| Test Name: Dom-size
| Usage:

::

  N/A

| Description:

N/A



netwerk/test/perf
-----------------
Performance tests from the 'network/test/perf' folder.

perftest_http3_cloudflareblog.js
================================

User-journey live site test for cloudflare blog.

| Owner: Network Team
| Test Name: cloudflare
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_facebook_scroll.js
=================================

Measures the number of requests per second after a scroll.

| Owner: Network Team
| Test Name: facebook-scroll
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_google_image.js
==============================

Measures the number of images per second after a scroll.

| Owner: Network Team
| Test Name: g-image
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_google_search.js
===============================

User-journey live site test for google search

| Owner: Network Team
| Test Name: g-search
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_lucasquicfetch.js
================================

Measures the amount of time it takes to load a set of images.

| Owner: Network Team
| Test Name: lq-fetch
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_youtube_watch.js
===============================

Measures quality of the video being played.

| Owner: Network Team
| Test Name: youtube-noscroll
| Usage:

::

  N/A

| Description:

N/A


perftest_http3_youtube_watch_scroll.js
======================================

Measures quality of the video being played.

| Owner: Network Team
| Test Name: youtube-scroll
| Usage:

::

  N/A

| Description:

N/A



testing/performance
-------------------
Performance tests from the 'testing/performance' folder.

perftest_bbc_link.js
====================

Measures time to load BBC homepage

| Owner: Performance Team
| Test Name: BBC Link
| Usage:

::

  N/A

| Description:

N/A


perftest_facebook.js
====================

Measures time to log in to Facebook

| Owner: Performance Team
| Test Name: Facebook
| Usage:

::

  N/A

| Description:

N/A


perftest_jsconf_cold.js
=======================

Measures time to load JSConf page (cold)

| Owner: Performance Team
| Test Name: JSConf (cold)
| Usage:

::

  N/A

| Description:

N/A


perftest_jsconf_warm.js
=======================

Measures time to load JSConf page (warm)

| Owner: Performance Team
| Test Name: JSConf (warm)
| Usage:

::

  N/A

| Description:

N/A


perftest_politico_link.js
=========================

Measures time to load Politico homepage

| Owner: Performance Team
| Test Name: Politico Link
| Usage:

::

  N/A

| Description:

N/A


perftest_android_view.js
========================

Measures cold process view time

| Owner: Performance Team
| Test Name: VIEW
| Usage:

::

  ./mach perftest testing/performance/perftest_android_view.js
  --android-install-apk ~/fenix.v2.fennec-nightly.2020.04.22-arm32.apk
  --hooks testing/performance/hooks_android_view.py --android-app-name
  org.mozilla.fenix --perfherder-metrics processLaunchToNavStart

| Description:

This test launches the appropriate android app, simulating a opening a
link through VIEW intent workflow. The application is launched with
the intent action android.intent.action.VIEW loading a trivially
simple website. The reported metric is the time from process start to
navigationStart, reported as processLaunchToNavStart


perftest_youtube_link.js
========================

Measures time to load YouTube video

| Owner: Performance Team
| Test Name: YouTube Link
| Usage:

::

  N/A

| Description:

N/A


perftest_android_main.js
========================

Measures the time from process start until the Fenix main activity
(HomeActivity) reports Fully Drawn

| Owner: Performance Team
| Test Name: main
| Usage:

::

  ./mach perftest testing/performance/perftest_android_main.js --android
  --flavor mobile-browser --hooks
  testing/performance/hooks_home_activity.py --perfherder
  --android-app-name org.mozilla.fenix --android-activity .App
  --android-install-apk ~/Downloads/fenix.apk --android-clear-logcat
  --android-capture-logcat logcat --androidlog-first-timestamp ".*Start
  proc.*org.mozilla.fenix.*.App.*" --androidlog-second-timestamp
  ".*Fully drawn.*org.mozilla.fenix.*" --androidlog-subtest-name "MAIN"
  --androidlog

| Description:

This test launches Fenix to its main activity (HomeActivity). The
application logs "Fully Drawn" when the activity is drawn. Using the
android log transformer we measure the time from process start to this
event.


perftest_pageload.js
====================

Measures time to load mozilla page

| Owner: Performance Team
| Test Name: pageload
| Usage:

::

  N/A

| Description:

N/A



If you have any questions, please see this `wiki page <https://wiki.mozilla.org/TestEngineering/Performance#Where_to_find_us>`_.
