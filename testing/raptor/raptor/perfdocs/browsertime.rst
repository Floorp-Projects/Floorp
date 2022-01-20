##################
Raptor Browsertime
##################

.. contents::
   :depth: 2
   :local:

Browsertime is a harness for running performance tests, similar to Mozilla's Raptor testing framework. Browsertime is written in Node.js and uses Selenium WebDriver to drive multiple browsers including Chrome, Chrome for Android, Firefox, and Firefox for Android and GeckoView-based vehicles.

Source code:

- Our current Browsertime version uses the canonical repo.
- In-tree: https://searchfox.org/mozilla-central/source/tools/browsertime and https://searchfox.org/mozilla-central/source/taskcluster/scripts/misc/browsertime.sh

Running Locally
---------------

**Prerequisites**

- A local mozilla repository clone with a `successful Firefox build <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`_ completed

Setup
-----

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
--------------------------

Page-load tests
---------------
There are two ways to run performance tests through browsertime listed below. **Note that ``./mach browsertime`` should not be used when debugging performance issues with profiles as it does not do symbolication.**

* Raptor-Browsertime (recommended):

::

  ./mach raptor --browsertime -t google-search

* Browsertime-"native":

::

    ./mach browsertime https://www.sitespeed.io --firefox.binaryPath '/Users/{userdir}/moz_src/mozilla-unified/obj-x86_64-apple-darwin18.7.0/dist/Nightly.app/Contents/MacOS/firefox'

Benchmark tests
---------------
* Raptor-wrapped:

::

  ./mach raptor -t raptor-speedometer --browsertime

Running on Android
------------------
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
------------------------
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
-------------

`Browsertime docs <https://github.com/mozilla/browsertime/tree/master/docs/examples>`_

Running Browsertime on Try
--------------------------
You can run all of our browsertime pageload tests through ``./mach try fuzzy --full``. We use chimera mode in these tests which means that both cold and warm pageload variants are running at the same time.

For example:

::

  ./mach try fuzzy -q "'g5 'imdb 'geckoview 'vismet '-wr 'shippable"

Retriggering Browsertime Visual Metrics Tasks
---------------------------------------------

You can retrigger Browsertime tasks just like you retrigger any other tasks from Treeherder (using the retrigger buttons, add-new-jobs, retrigger-multiple, etc.).

When you retrigger the Browsertime test task, it will trigger a new vismet task as well. If you retrigger a Browsertime vismet task, then it will cause the test task to be retriggered and a new vismet task will be produced from there. This means that both of these tasks are treated as "one" task when it comes to retriggering them.

There is only one path that still doesn't work for retriggering Browsertime tests and that happens when you use ``--rebuild X`` in a try push submission.

For details on how we previously retriggered visual metrics tasks see `VisualMetrics <https://wiki.mozilla.org/TestEngineering/Performance/Raptor/VisualMetrics>`_ (this will stay here for a few months just in case).

Gecko Profiling with Browsertime
--------------------------------

To run gecko profiling using Raptor-Browsertime you can add the ``--gecko-profile`` flag to any command and you will get profiles from the test (with the profiler page opening in the browser automatically). This method also performs symbolication for you. For example:

::

  ./mach raptor --browsertime -t amazon --gecko-profile

Note that vanilla Browsertime does support Gecko Profiling but **it does not symbolicate the profiles** so it is **not recommended** to use for debugging performance regressions/improvements.

Upgrading Browsertime In-Tree
-----------------------------
To upgrade the browsertime version used in-tree you can run, then commit the changes made to ``package.json`` and ``package-lock.json``:

::

  ./mach browsertime --update-upstream-url <TARBALL-URL>

Here is a sample URL that we can update to: https://github.com/sitespeedio/browsertime/tarball/89771a1d6be54114db190427dbc281582cba3d47

To test the upgrade, run a raptor test locally (with and without visual-metrics ``--browsertime-visualmetrics`` if possible) and test it on try with at least one test on desktop and mobile.

Finding the Geckodriver Being Used
----------------------------------
If you're looking for the latest geckodriver being used there are two ways:
* Find the latest one from here: https://treeherder.mozilla.org/jobs?repo=mozilla-central&searchStr=geckodriver
* Alternatively, if you're trying to figure out which geckodriver a given CI task is using, you can click on the browsertime task in treeherder, and then click on the ``Task`` id in the bottom left of the pop-up interface. Then in the window that opens up, click on `See more` in the task details tab on the left, this will show you the dependent tasks with the latest toolchain-geckodriver being used. There's an Artifacts drop down on the right hand side for the toolchain-geckodriver task that you can find the latest geckodriver in.

If you're trying to test Browsertime with a new geckodriver, you can do either of the following:
* Request a new geckodriver build in your try run (i.e. through ``./mach try fuzzy``).
* Trigger a new geckodriver in a try push, then trigger the browsertime tests which will then use the newly built version in the try push.

Comparing Before/After Browsertime Videos
-----------------------------------------

We have some scripts that can produce side-by-side comparison videos for you of the worst pairing of videos. You can find the script here: https://github.com/mozilla/mozperftest-tools#browsertime-side-by-side-video-comparisons

Once the side-by-side comparison is produced, the video on the left is the old/base video, and the video on the right is the new video.
