###################
Raptor WebExtension
###################

  **Note: WebExtension is being deprecated, new tests should be written using Browsertime**

.. contents::
   :depth: 2
   :local:

WebExtension Page-Load Tests
----------------------------

Page-load tests involve loading a specific web page and measuring the load performance (i.e. `time-to-first-non-blank-paint <https://wiki.mozilla.org/TestEngineering/Performance/Glossary#First_Non-Blank_Paint_.28fnbpaint.29>`_, first-contentful-paint, `dom-content-flushed <https://wiki.mozilla.org/TestEngineering/Performance/Glossary#DOM_Content_Flushed_.28dcf.29>`_).

For page-load tests by default, instead of using live web pages for performance testing, Raptor uses a tool called `Mitmproxy <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/Mitmproxy>`_. Mitmproxy allows us to record and playback test pages via a local Firefox proxy. The Mitmproxy recordings are stored on `tooltool <https://github.com/mozilla/build-tooltool>`_ and are automatically downloaded by Raptor when they are required for a test. Raptor uses mitmproxy via the `mozproxy <https://searchfox.org/mozilla-central/source/testing/mozbase/mozproxy>`_ package.

There are two different types of Raptor page-load tests: warm page-load and cold page-load.

Warm Page-Load
==============
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
==============
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
================
It is possible to use live web pages for the page-load tests instead of using the mitmproxy recordings. To do this, add ``--live`` to your command line or select one of the 'live' variants when running ``./mach try fuzzy --full``.

Disabling Alerts
================
It is possible to disable alerting for all our performance tests. Open the target test manifest such as the raptor-tp6*.ini file (`Raptor tests folder <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/tests>`_), and make sure there are no ``alert_on`` specifications.

When it's removed there will no longer be a ``shouldAlert`` field in the output Perfherder data (you can find the `schema here <https://searchfox.org/mozilla-central/source/testing/mozharness/external_tools/performance-artifact-schema.json#68,165>`_). As long as ``shouldAlert`` is not in the data, no alerts will be generated. If you need to also disable code sheriffing for the test, then you need to change the tier of the task to 3.

High value tests
================

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
----------------------------

Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI.

Scenario Tests
--------------

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
=========================
Prerequisites
^^^^^^^^^^^^^


#. rooted (i.e. superuser-capable), bootloader-unlocked Moto G5 or Google Pixel 2: internal (for now) `test-device setup doc. <https://docs.google.com/document/d/1XQLtvVM2U3h1jzzzpcGEDVOp4jMECsgLYJkhCfAwAnc/edit>`_
#. set up to run Raptor from a Firefox source tree (see `Running Locally <https://wiki.mozilla.org/Performance_sheriffing/Raptor#Running_Locally>`_)
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
==============================

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
=================

Pushing to Try server
=====================
As an example, a relatively good cross-sampling of builds can be seen in https://hg.mozilla.org/try/rev/6c07631a0c2bf56b51bb82fd5543d1b34d7f6c69.

* Include both G5 Android 7 (hw-g5-7-0-arm7-api-16/) *and* Pixel 2 Android 8 (p2-8-0-android-aarch64/) target platforms
* pgo builds tend to be -- from my limited empirical evidence -- about 10 - 15 minutes longer to complete than their opt counterparts

Dashboards
==========

See `performance results <https://wiki.mozilla.org/TestEngineering/Performance/Results>`_ for our various dashboards.

Running WebExtension Locally
----------------------------

WebExtension Prerequisites
==========================

In order to run Raptor on a local machine, you need:

* A local mozilla repository clone with a `successful Firefox build <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`_ completed
* Git needs to be in the path in the terminal/window in which you build Firefox / run Raptor, as Raptor uses Git to check-out a local copy for some of the performance benchmarks' sources.
* If you plan on running Raptor tests on Google Chrome, you need a local install of Google Chrome and know the path to the chrome binary
* If you plan on running Raptor on Android, your Android device must already be set up (see more below in the Android section)

Getting a List of Raptor Tests
==============================

To see which Raptor performance tests are currently available on all platforms, use the 'print-tests' option, e.g.:

::

  $ ./mach raptor --print-tests

That will output all available tests on each supported app, as well as each subtest available in each suite (i.e. all the pages in a specific page-load tp6* suite).

Running on Firefox
==================

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
============================================

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
========================

To run Raptor locally on Google Chrome, make sure you already have a local version of Google Chrome installed, and then from within your mozilla-repo run:

::

  $ ./mach raptor --test <raptor-test-name> --app=chrome --binary="<path to google chrome binary>"

For example, to run the raptor-speedometer benchmark on Google Chrome use:

::

  $ ./mach raptor --test raptor-speedometer --app=chrome --binary="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome

Raptor test results will be found locally in <your-repo>/testing/mozharness/build/raptor.json.

Page-Timeouts
=============

On different machines the Raptor tests will run at different speeds. The default page-timeout is defined in each Raptor test INI file. On some machines you may see a test failure with a 'raptor page-timeout' which means the page-load timed out, or the benchmark test iteration didn't complete, within the page-timeout limit.

You can override the default page-timeout by using the --page-timeout command-line arg. In this example, each test page in tp6-1 will be given two minutes to load during each page-cycle:

::

  ./mach raptor --test raptor-tp6-1 --page-timeout 120000

If an iteration of a benchmark test is not finishing within the allocated time, increase it by:

::

  ./mach raptor --test raptor-speedometer --page-timeout 600000

Page-Cycles
===========

Page-cycles is the number of times a test page is loaded (for page-load tests); for benchmark tests, this is the total number of iterations that the entire benchmark test will be run. The default page-cycles is defined in each Raptor test INI file.

You can override the default page-cycles by using the --page-cycles command-line arg. In this example, the test page will only be loaded twice:

::

  ./mach raptor --test raptor-tp6-google-firefox --page-cycles 2

Running Page-Load Tests on Live Sites
=====================================
To use live pages instead of page recordings, just edit the `Raptor tp6* test INI <https://searchfox.org/mozilla-central/source/testing/raptor/raptor/tests>`_ file and add the following attribute either at the top (for all pages in the suite) or under an individual page/subtest heading:

  use_live_pages = true

With that setting, Raptor will not start the playback tool (i.e. Mitmproxy) and will not turn on the corresponding browser proxy, therefore forcing the test page to load live.

Running Raptor on Try
---------------------

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
=============================

The Raptor performance tests run on dedicated hardware (the same hardware that the Talos performance tests use). See the `performance platforms <https://wiki.mozilla.org/TestEngineering/Performance/Platforms>`_ for more details.

Profiling Raptor Jobs
---------------------

Raptor tests are able to create Gecko profiles which can be viewed in `profiler.firefox.com. <https://profiler.firefox.com/>`__ This is currently only supported when running Raptor on Firefox desktop.

Nightly Profiling Jobs in Production
====================================
We have Firefox desktop Raptor jobs with Gecko-profiling enabled running Nightly in production on Mozilla Central (on Linux64, Win10, and OSX). This provides a steady cache of Gecko profiles for the Raptor tests. Search for the `"Rap-Prof" treeherder group on Mozilla Central <https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&searchStr=Rap-Prof>`_.

Profiling Locally
=================

To tell Raptor to create Gecko profiles during a performance test, just add the '--gecko-profile' flag to the command line, i.e.:

::

  $ ./mach raptor --test raptor-sunspider --gecko-profile

When the Raptor test is finished, you will be able to find the resulting gecko profiles (ZIP) located locally in:

     mozilla-central/testing/mozharness/build/blobber_upload_dir/

Note: While profiling is turned on, Raptor will automatically reduce the number of pagecycles to 3. If you wish to override this, add the --page-cycles argument to the raptor command line.

Raptor will automatically launch Firefox and load the latest Gecko profile in `profiler.firefox.com <https://profiler.firefox.com>`__. To turn this feature off, just set the DISABLE_PROFILE_LAUNCH=1 env var.

If auto-launch doesn't work for some reason, just start Firefox manually and browse to `profiler.firefox.com <https://profiler.firefox.com>`__, click on "Browse" and select the Raptor profile ZIP file noted above.

If you're on Windows and want to profile a Firefox build that you compiled yourself, make sure it contains profiling information and you have a symbols zip for it, by following the `directions on MDN <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Profiling_with_the_Built-in_Profiler_and_Local_Symbols_on_Windows#Profiling_local_talos_runs>`_.

Profiling on Try Server
=======================

To turn on Gecko profiling for Raptor test jobs on try pushes, just add the '--gecko-profile' flag to your try push i.e.:

::

  $ ./mach try fuzzy --gecko-profile

Then select the Raptor test jobs that you wish to run. The Raptor jobs will be run on try with profiling included. While profiling is turned on, Raptor will automatically reduce the number of pagecycles to 2.

See below for how to view the gecko profiles from within treeherder.

Customizing the profiler
========================
If the default profiling options are not enough, and further information is needed the gecko profiler can be customized.

Enable profiling of additional threads
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In some cases it will be helpful to also measure threads which are not part of the default set. Like the **MediaPlayback** thread. This can be accomplished by using:

#. the **gecko_profile_threads** manifest entry, and specifying the thread names as comma separated list
#. the **--gecko-profile-thread** argument for **mach** for each extra thread to profile

Add Profiling to Previously Completed Jobs
==========================================

Note: You might need treeherder 'admin' access for the following.

Gecko profiles can now be created for Raptor performance test jobs that have already completed in production (i.e. mozilla-central) and on try. To repeat a completed Raptor performance test job on production or try, but add gecko profiling, do the following:

#. In treeherder, select the symbol for the completed Raptor test job (i.e. 'ss' in 'Rap-e10s')
#. Below, and to the left of the 'Job Details' tab, select the '...' to show the menu
#. On the pop-up menu, select 'Create Gecko Profile'

The same Raptor test job will be repeated but this time with gecko profiling turned on. A new Raptor test job symbol will be added beside the completed one, with a '-p' added to the symbol name. Wait for that new Raptor profiling job to finish. See below for how to view the gecko profiles from within treeherder.

Viewing Profiles on Treeherder
==============================
When the Raptor jobs are finished, to view the gecko profiles:

#. In treeherder, select the symbol for the completed Raptor test job (i.e. 'ss' in 'Rap-e10s')
#. Click on the 'Job Details' tab below
#. The Raptor profile ZIP files will be listed as job artifacts;
#. Select a Raptor profile ZIP artifact, and click the 'view in Firefox Profiler' link to the right

Recording Pages for Raptor Pageload Tests
-----------------------------------------

Raptor pageload tests ('tp6' and 'tp6m' suites) use the `Mitmproxy <https://mitmproxy.org/>`__ tool to record and play back page archives. For more information on creating new page playback archives, please see `Raptor and Mitmproxy <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/Mitmproxy>`__.

Performance Tuning for Android devices
--------------------------------------

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
