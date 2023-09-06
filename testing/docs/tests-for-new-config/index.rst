Turning on Firefox tests for a new configuration
================================================

You are ready to go with turning on Firefox tests for a new config.  Once you
get to this stage, you will have seen a try push with all the tests running
(many not green) to verify some tests pass and there are enough machines
available to run tests.

For the purpose of this document, assume you are tasked with upgrading Windows
10 OS from 1803 -> 1903. To simplify this we can call this `windows_1903`, and
we need to:

 * push to try
 * analyze test failures
 * disable tests in manifests
 * repeat try push until no failures
 * land changes and turn on tests
 * turn on run only failures
 * file bugs for test failures

There are many edge cases, and I will outline them inside each step.


Push to Try Server
------------------

As you have new machines (or cloud instances) available with the updated
OS/config, it is time to push to try.

In order to run all tests, we would need to execute:
  ``./mach try fuzzy --no-artifact -q 'test-windows !-raptor- !-talos- --rebuild 5``

There are a few exceptions here:

 * Perf tests don't need to be run (hence the ``!-raptor- !-talos-``)
 * Need to make sure we are not building with artifact builds (hence the
   ``--no-artifact``)
 * There are jobs hidden behind tier-3, some for a good reason (code coverage is
   a good example, but fission tests might not be green)

 The last piece to sort out is running on the new config, here are some
 considerations for new configs:

  * duplicated jobs (i.e. fission, a11y-checks), you can just run those specific
    tasks: ``./mach try fuzzy --no-artifact -q 'test-windows fission --rebuild
    5``
  * new OS/hardware (i.e. aarch64, os upgrade), you need to reference the new
    hardware, typically this is with ``--worker-override``: ``./mach try fuzzy
    --no-artifact -q 'test-windows --rebuild 5 --worker-override
    t-win10-64=gecko-t/t-win10-64-1903``

    * the risk here is a scenario where hardware is limited, then ``--rebuild
      5`` will create too many tasks and some will expire.
    * in low hardware situations, either run a subset of tests (i.e.
      web-platform-tests, mochitest), or ``--rebuild 2`` and repeat.


Analyze Test Failures
---------------------

A try push will take many hours, it is best to push in the afternoon, ensure
some jobs are running, then come back the next day.

The best way to look at test failures is to use Push Health to avoid misleading
data.  Push Health will bucket failures into possible regressions, known
regression, etc. When looking at 5 data points (from ``--rebuild 5``), this
will filter out intermittent failures.

There are many reasons you might have invalid or misleading data:

 # Tests fail intermittently, we need a pattern to know if it is consistent or
 intermittent.
 # We still want to disable high frequency intermittent tests, those are just
 annoying.
 # You could be pushing off a bad base revision (regression or intermittent that
 comes from the base revision).
 # The machines you run on could be bad, skewing the data.
 # Infrastructure problems could cause jobs to fail at random places, repeated
 jobs filter that out.
 # Some failures could affect future tests in the same browser session or tasks.
 # If a crash occurs, or we timeout- it is possible that we will not run all of
 the tests in the task, therefore believing a test was run 5 times, but maybe it
 was only run once (and failed), or never run at all.
 # Task failures that do not have a test name (leak on shutdown, crash on
 shutdown, timeout on shutdown, etc.)

That is a long list of reasons to not trust the data, luckily most of the time
using ``--rebuild 5`` will give us enough data to give enough confidence we
found all failures and can ignore random/intermittent failures.

Knowing the reasons for misleading data, here is a way to use `Push Health
<https://treeherder.mozilla.org/push-health/push?revision=abaff26f8e084ac719bea0438dba741ace3cf5d8&repo=try&testGroup=pr>`__.

 * Alternatively, you could use the `API
   <https://treeherder.mozilla.org/api/project/try/push/health/?revision=abaff26f8e084ac719bea0438dba741ace3cf5d8>`__
   to get raw data and work towards building a tool
 * If you write a tool, you need to parse the resulting JSON file and keep in
   mind to build a list of failures and match it with a list of jobnames to find
   how many times the job ran and failed/passed.

The main goal here is to know what <path>/<filenames> are failing, and having a
list of those.  Ideally you would record some additional information like
timeout, crash, failure, etc.  In the end you might end up with::

     dom/html/test/test_fullscreen-api.html, scrollbar
     gfx/layers/apz/test/mochitest/test_group_hittest.html, scrollbar
     image/test/mochitest/test_animSVGImage.html, timeout
     browser/base/content/test/general/browser_restore_isAppTab.js, crashed




Disable Tests in the Manifest Files
-----------------------------------

This part of the process can seem tedious and is difficult to automate without
making our manifests easier to access programatically.

The code sheriffs have been using `this documentation
<https://wiki.mozilla.org/Auto-tools/Projects/Stockwell/disable-recommended>`__
for training and reference when they disable intermittents.

First you need to add a keyword to be available in the manifest (e.g. ``skip-if
= windows_1903``).

There are many exceptions, the bulk of the work will fall into one of 4
categories:

 # `manifestparser <mochitest_xpcshell_manifest_keywords>`_: \*.ini (mochitest*,
 firefox-ui, marionette, xpcshell) easy to edit by adding a ``fail-if =
 windows_1903 # <comment>``, a few exceptions here
 # `reftest <reftest_manifest_keywords>`_: \*.list (reftest, crashtest) need to
 add a ``fuzzy-if(windows_1903, A, B)``, this is more specific
 # web-platform-test: testing/web-platform/meta/\*\*.ini (wpt, wpt-reftest,
 etc.) need to edit/add testing/web-platform/meta/<path>/<testname>.ini, and add
 expected results
 # Other (compiled tests, jsreftest, etc.) edit source code, ask for help.

Basically we want to take every non intermittent failure found from push health
and edit the manifest, this typically means:

 * Finding the proper manifest.
 * Adding the right text to the manifest.

To find the proper manifest, it is typically <path>/<harness>.[ini|list].
There are exceptions and if in doubt use searchfox.org/ to find the manifest
which contains the testname.

Once you have the manifest, open it in an editor, and search for the exact test
name (there could be similar named tests).

Rerun Try Push, Repeat as Necessary
-----------------------------------

It is important to test your changes and for a new platform that will be
sheriffed, to rerun all the tests at scale.

With your change in a commit, push again to try with ``--rebuild 5`` and come
back the next day.

As there are so many edge cases, it is quite likely that you will have more
failures, mentally plan on 3 iterations of this, where each iteration has fewer
failures.

Once you get a full push to show no persistent failures, it is time to land
those changes and turn on the new tests. There is a large risk here that the
longer you take to find all failures, the greater the chance of:

  * Bitrot of your patch
  * New tests being added which could fail on your config
  * Other edits to tests/tools which could affect your new config

Since the new config process is designed to find failures fast and get the
changes landed fast, we do not need to ask developers for review, that comes
after the new config is running successfully where we notify the teams of what
tests are failing.

Land Changes and Turn on Tests
------------------------------

After you have a green test run, it is time to land the patches.  There could
be changes needed to the taskgraph in order to add the new hardware type and
duplicate tests to run on both the old and the new, or create a new variant and
denote which tests to run on that variant.

Using our example of ``windows_1903``, this would be a new worker type that
would require these edits:

 * `transforms/tests.py <https://searchfox.org/mozilla-central/source/taskcluster/taskgraph/transforms/tests.py#97>`__ (duplicate windows 10 entries)
 * `test-platforms.py <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/test-platforms.yml#229>`__ (copy windows10 debug/opt/shippable/asan entries and make win10_1903)
 * `test-sets.py <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/test-sets.yml#293>`__ (ideally you need nothing, otherwise copy ``windows-tests`` and edit the test list)

In general this should allow you to have tests scheduled with no custom flags
in try server and all of these will be scheduled by default on
``mozilla-central``, ``autoland``, and ``release-branches``.

Turn on Run Only Failures
-------------------------

Now that we have tests running regularly, the next step is to take all the
disabled tests and run them in the special failures job.

We have a basic framework created, but for every test harness (i.e. xpcshell,
mochitest-gpu, browser-chrome, devtools, web-platform-tests, crashtest, etc.),
there will need to be a corresponding tier-3 job that is created.

TODO: point to examples of how to add this after we get our first jobs running.

File Bugs for Test Failures
---------------------------

Once the failure jobs are running on mozilla-central, now we have full coverage
and the ability to run tests on try server.  There could be >100 tests that are
marked as ``fail-if`` and that would take a lot of time to file bugs.  Instead
we will file a bug for each manifest that is edited, typically this reduces the
bugs to about 40% the total tests (average out to 2.5 test failures/manifest).

When filing the bug, indicate the timeline, how to run the failure, link to the
bug where we created the config, describe briefly the config change (i.e.
upgrade windows 10 rom version 1803 to 1903), and finally needinfo the triage
owner indicating this is a heads up and these tests are running reguarly on
mozilla-central for the next 6-7 weeks.
