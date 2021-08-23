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

{documentation}

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

* contact: padenot
* type: benchmark
* browsers: Firefox desktop, Chrome desktop
* measuring: Rendering speed of various synthetic Web Audio API workloads
* reporting: The time time it took to render the audio of each test case, and a
  geometric mean of the full test suite. Lower is better
* data: Upstream is https://github.com/padenot/webaudio-benchmark/. Those
  benchmarks are run by other projects. Upstream is vendored in mozilla-central
  via an simple update script, at `third_party/webkit/PerformanceTests/webaudio`

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
