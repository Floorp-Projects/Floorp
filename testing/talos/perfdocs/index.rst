=====
Talos
=====

Talos is a cross-platform Python performance testing framework that is specifically for
Firefox on desktop. New performance tests should be added to the newer framework
`mozperftest </testing/perfdocs/mozperftest.html>`_ unless there are limitations
there (highly unlikely) that make it absolutely necessary to add them to Talos. Talos is
named after the `bronze automaton from Greek myth <https://en.wikipedia.org/wiki/Talos>`_.

.. contents::
   :depth: 1
   :local:

Talos tests are run in a similar manner to xpcshell and mochitests. They are started via
the command :code:`mach talos-test`. A `python script <https://searchfox.org/mozilla-central/source/testing/talos>`_
then launches Firefox, which runs the tests via JavaScript special powers. The test timing
information is recorded in a text log file, e.g. :code:`browser_output.txt`, and then processed
into the `JSON format supported by Perfherder <https://searchfox.org/mozilla-central/source/testing/mozharness/external_tools/performance-artifact-schema.json>`_.

Talos bugs can be filed in `Testing::Talos <https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Talos>`_.

Talos infrastructure is still mostly documented `on the Mozilla Wiki <https://wiki.mozilla.org/TestEngineering/Performance/Talos>`_.
In addition, there are plans to surface all of the individual tests using PerfDocs.
This work is tracked in `Bug 1674220 <https://bugzilla.mozilla.org/show_bug.cgi?id=1674220>`_.

Examples of current Talos runs can be `found in Treeherder by searching for "Talos" <https://treeherder.mozilla.org/jobs?repo=autoland&searchStr=Talos>`_.
If none are immediately available, then scroll to the bottom of the page and load more test
runs. The tests all share a group symbol starting with a :code:`T`, for example
:code:`T(c d damp g1)` or :code:`T-gli(webgl)`.

Running Talos Locally
*********************

Running tests locally is most likely only useful for debugging what is going on in a test,
as the test output is only reported as raw JSON. The CLI is documented via:

.. code-block:: bash

    ./mach talos-test --help

To quickly try out the :code:`./mach talos-test` command, the following can be run to do a
single run of the DevTools' simple netmonitor test.

.. code-block:: bash

    # Run the "simple.netmonitor" test very quickly with 1 cycle, and 1 page cycle.
    ./mach talos-test --activeTests damp --subtests simple.netmonitor --cycles 1 --tppagecycles 1


The :code:`--print-suites` and :code:`--print-tests` are two helpful command flags to
figure out what suites and tests are available to run.

.. code-block:: bash

    # Print out the suites:
    ./mach talos-test --print-suites

    # Available suites:
    #  bcv                          (basic_compositor_video)
    #  chromez                      (about_preferences_basic:tresize)
    #  dromaeojs                    (dromaeo_css:kraken)
    #  flex                         (tart_flex:ts_paint_flex)
    # ...

    # Run all of the tests in the "bcv" test suite:
    ./mach talos-test --suite bcv

    # Print out the tests:
    ./mach talos-test --print-tests

    # Available tests:
    # ================
    #
    # a11yr
    # -----
    # This test ensures basic a11y tables and permutations do not cause
    # performance regressions.
    #
    # ...

    # Run the tests in "a11yr" listed above
    ./mach talos-test --activeTests a11yr

Running Talos on Try
********************

Talos runs can be generated through the mach try fuzzy finder:

.. code-block:: bash

    ./mach try fuzzy

The following is an example output at the time of this writing. Refine the query for the
platform and test suites of your choosing.

.. code-block::

    | test-windows10-64-qr/opt-talos-bcv-swr-e10s
    | test-linux64-shippable/opt-talos-webgl-e10s
    | test-linux64-shippable/opt-talos-other-e10s
    | test-linux64-shippable-qr/opt-talos-g5-e10s
    | test-linux64-shippable-qr/opt-talos-g4-e10s
    | test-linux64-shippable-qr/opt-talos-g3-e10s
    | test-linux64-shippable-qr/opt-talos-g1-e10s
    | test-windows10-64/opt-talos-webgl-gli-e10s
    | test-linux64-shippable/opt-talos-tp5o-e10s
    | test-linux64-shippable/opt-talos-svgr-e10s
    | test-linux64-shippable/opt-talos-flex-e10s
    | test-linux64-shippable/opt-talos-damp-e10s
    > test-windows7-32/opt-talos-webgl-gli-e10s
    | test-linux64-shippable/opt-talos-bcv-e10s
    | test-linux64-shippable/opt-talos-g5-e10s
    | test-linux64-shippable/opt-talos-g4-e10s
    | test-linux64-shippable/opt-talos-g3-e10s
    | test-linux64-shippable/opt-talos-g1-e10s
    | test-linux64-qr/opt-talos-bcv-swr-e10s

      For more shortcuts, see mach help try fuzzy and man fzf
      select: <tab>, accept: <enter>, cancel: <ctrl-c>, select-all: <ctrl-a>, cursor-up: <up>, cursor-down: <down>
      1379/2967
    > talos

At a glance
***********

-  Tests are defined in
   `testing/talos/talos/test.py <https://searchfox.org/mozilla-central/source/testing/talos/talos/test.py>`__
-  Treeherder abbreviations are defined in
   `taskcluster/ci/test/talos.yml <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/talos.yml>`__
-  Suites are defined for production in
   `testing/talos/talos.json <https://searchfox.org/mozilla-central/source/testing/talos/talos.json>`__

Test lifecycle
**************

-  Taskcluster schedules `talos
   jobs <https://searchfox.org/mozilla-central/source/taskcluster/ci/test/talos.yml>`__
-  Taskcluster runs a Talos job on a hardware machine when one is
   available - this is bootstrapped by
   `mozharness <https://searchfox.org/mozilla-central/source/testing/mozharness/mozharness/mozilla/testing/talos.py>`__

   -  mozharness downloads the build, talos.zip (found in
      `talos.json <https://searchfox.org/mozilla-central/source/testing/talos/talos.json>`__),
      and creates a virtualenv for running the test.
   -  mozharness `configures the test and runs
      it <https://wiki.mozilla.org/TestEngineering/Performance/Talos/Running#How_Talos_is_Run_in_Production>`__
   -  After the test is completed the data is uploaded to
      `Perfherder <https://treeherder.mozilla.org/perf.html#/graphs>`__

-  Treeherder displays a green (all OK) status and has a link to
   `Perfherder <https://treeherder.mozilla.org/perf.html#/graphs>`__
-  13 pushes later,
   `analyze_talos.py <http://hg.mozilla.org/graphs/file/tip/server/analysis/analyze_talos.py>`__
   is ran which compares your push to the previous 12 pushes and next 12
   pushes to look for a
   `regression <https://wiki.mozilla.org/TestEngineering/Performance/Talos/Data#Regressions>`__

   -  If a regression is found, it will be posted on `Perfherder
      Alerts <https://treeherder.mozilla.org/perf.html#/alerts>`__

Test types
**********

There are two different species of Talos tests:

-  Startup_: Start up the browser and wait for either the load event or the paint event and exit, measuring the time
-  `Page load`_: Load a manifest of pages

In addition we have some variations on existing tests:

-  Heavy_: Run tests with the heavy user profile instead of a blank one
-  `Web extension`_: Run tests with a web extension to see the perf impact extension have
-  `Real-world WebExtensions`_: Run tests with a set of 5 popular real-world WebExtensions installed and enabled.

Some tests measure different things:

-  Paint_: These measure events from the browser like moz_after_paint, etc.
-  ASAP_: These tests go really fast and typically measure how many frames we can render in a time window
-  Benchmarks_: These are benchmarks that measure specific items and report a summarized score

Startup
=======

`Startup
tests <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/startup_test>`__
launch Firefox and measure the time to the onload or paint events. We
run this in a series of cycles (default to 20) to generate a full set of
data. Tests that currently are startup tests are:

-  `ts_paint <#ts_paint>`_
-  tpaint_
-  `tresize <#tresize>`_
-  `sessionrestore <#sessionrestore>`_
-  `sessionrestore_no_auto_restore <#sessionrestore_no_auto_restore>`_
-  `sessionrestore_many_windows <#sessionrestore_many_windows>`_

Page load
=========

Many of the talos tests use the page loader to load a manifest of pages.
These are tests that load a specific page and measure the time it takes
to load the page, scroll the page, draw the page etc. In order to run a
page load test, you need a manifest of pages to run. The manifest is
simply a list of URLs of pages to load, separated by carriage returns,
e.g.:

.. code-block:: none

   https://www.mozilla.org
   https://www.mozilla.com

Example:
`svgx.manifest <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/tests/svgx/svgx.manifest>`__

Manifests may also specify that a test computes its own data by
prepending a ``%`` in front of the line:

.. code-block:: none

   % https://www.mozilla.org
   % https://www.mozilla.com

Example:
`v8.manifest <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/tests/v8_7/v8.manifest>`__

The file you created should be referenced in your test config inside of
`test.py <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/test.py#l607>`__.
For example, open test.py, and look for the line referring to the test
you want to run:

.. code-block:: python

   tpmanifest = '${talos}/page_load_test/svgx/svgx.manifest'
   tpcycles = 1 # run a single cycle
   tppagecycles = 25 # load each page 25 times before moving onto the next page

Heavy
=====

All our testing is done with empty blank profiles, this is not ideal for
finding issues for end users. We recently undertook a task to create a
daily update to a profile so it is modern and relevant. It browses a
variety of web pages, and have history and cache to give us a more
realistic scenario.

The toolchain is documented on
`github <https://github.com/tarekziade/heavy-profile>`__ and was added
to Talos in `bug
1407398 <https://bugzilla.mozilla.org/show_bug.cgi?id=1407398>`__.

Currently we have issues with this on windows (takes too long to unpack
the files from the profile), so we have turned this off there. Our goal
is to run this on basic pageload and startup tests.

Web extension
=============

Web Extensions are what Firefox has switched to and there are different
code paths and APIs used vs addons. Historically we don't test with
addons (other than our test addons) and are missing out on common
slowdowns. In 2017 we started running some startup and basic pageload
tests with a web extension in the profile (`bug
1398974 <https://bugzilla.mozilla.org/show_bug.cgi?id=1398974>`__). We
have updated the Extension to be more real world and will continue to do
that.

Real-world WebExtensions
========================

We've added a variation on our test suite that automatically downloads,
installs and enables 5 popular WebExtensions. This is used to measure
things like the impact of real-world WebExtensions on start-up time.

Currently, the following extensions are installed:

-  Adblock Plus (3.5.2)
-  Cisco Webex Extension (1.4.0)
-  Easy Screenshot (3.67)
-  NoScript (10.6.3)
-  Video DownloadHelper (7.3.6)

Note that these add-ons and versions are "pinned" by being held in a
compressed file that's hosted in an archive by our test infrastructure
and downloaded at test runtime. To update the add-ons in this set, one
must provide a new ZIP file to someone on the test automation team. See
`this comment in
Bugzilla <https://bugzilla.mozilla.org/show_bug.cgi?id=1575089#c3>`__.

Paint
=====

Paint tests are measuring the time to receive both the
`MozAfterPaint <https://developer.mozilla.org/en-US/docs/Web/Events/MozAfterPaint>`__
and OnLoad event instead of just the OnLoad event. Most tests now look
for this unless they are an ASAP test, or an internal benchmark

ASAP
====

We have a variety of tests which we now run in ASAP mode where we render
as fast as possible (disabling vsync and letting the rendering iterate
as fast as it can using \`requestAnimationFrame`). In fact we have
replaced some original tests with the 'x' versions to make them measure.
We do this with RequestAnimationFrame().

ASAP tests are:

-  `basic_compositor_video <#basic_compositor_video>`_
-  `displaylist_mutate <#displaylist_mutate>`_
-  `glterrain <#glterrain>`_
-  `rasterflood_svg <#rasterflood_svg>`_
-  `rasterflood_gradient <#rasterflood_gradient>`_
-  `tsvgx <#tsvgx>`_
-  `tscrollx <#tscrollx>`_
-  `tp5o_scroll <#tp5o_scroll>`_
-  `tabswitch <#tabswitch>`_
-  `tart <#tart>`_

Benchmarks
==========

Many tests have internal benchmarks which we report as accurately as
possible. These are the exceptions to the general rule of calculating
the suite score as a geometric mean of the subtest values (which are
median values of the raw data from the subtests).

Tests which are imported benchmarks are:

-  `ARES6 <#ARES6>`_
-  `dromaeo <#dromaeo>`_
-  `JetStream <#JetStream>`_
-  `kraken <#kraken>`_
-  `motionmark <#motionmark>`_
-  `stylebench <#stylebench>`_

Row major vs. column major
==========================

To get more stable numbers, tests are run multiple times. There are two
ways that we do this: row major and column major. Row major means each
test is run multiple times and then we move to the next test (and run it
multiple times). Column major means that each test is run once one after
the other and then the whole sequence of tests is run again.

More background information about these approaches can be found in Joel
Maher's `Reducing the Noise in
Talos <https://elvis314.wordpress.com/2012/03/12/reducing-the-noise-in-talos/>`__
blog post.

Page sets
*********

We run our tests 100% offline, but serve pages via a webserver. Knowing
this we need to store and make available the offline pages we use for
testing.

tp5pages
========

Some tests make use of a set of 50 "real world" pages, known as the tp5n
set. These pages are not part of the talos repository, but without them
the tests which use them won't run.

-  To add these pages to your local setup, download the latest tp5n zip
   from `tooltool <https://mozilla-releng.net/tooltool/>`__, and extract
   it such that ``tp5n`` ends up as ``testing/talos/talos/tests/tp5n``.
   You can also obtain it by running a talos test locally to get the zip
   into ``testing/talos/talos/tests/``, i.e ``./mach talos-test --suite damp``
-  see also `tp5o <#tp5o>`_.

{documentation}

Extra Talos Tests
*****************

.. contents::
    :depth: 1
    :local:

about_newtab_with_snippets
==========================

.. note::

   add test details

File IO
-------

File IO is tested using the tp5 test set in the `xperf`_
test.

Possible regression causes
~~~~~~~~~~~~~~~~~~~~~~~~~~

-  **nonmain_startup_fileio opt (with or without e10s) windows7-32** –
   `bug
   1274018 <https://bugzilla.mozilla.org/show_bug.cgi?id=1274018>`__
   This test seems to consistently report a higher result for
   mozilla-central compared to Try even for an identical revision due to
   extension signing checks. In other words, if you are comparing Try
   and Mozilla-Central you may see a false-positive regression on
   perfherder. Graphs:
   `non-e10s <https://treeherder.mozilla.org/perf.html#/graphs?timerange=604800&series=%5Bmozilla-central,e5f5eaa174ef22fdd6b6e150e8c450aa827c2ff6,1,1%5D&series=%5Btry,e5f5eaa174ef22fdd6b6e150e8c450aa827c2ff6,1,1%5D>`__
   `e10s <https://treeherder.mozilla.org/perf.html#/graphs?series=%5B%22mozilla-central%22,%222f3af3833d55ff371ecf01c41aeee1939ef3a782%22,1,1%5D&series=%5B%22try%22,%222f3af3833d55ff371ecf01c41aeee1939ef3a782%22,1,1%5D&timerange=604800>`__

Xres (X Resource Monitoring)
----------------------------

A memory metric tracked during tp5 test runs. This metric is sampled
every 20 seconds. This metric is collected on linux only.

`xres man page <https://linux.die.net/man/3/xres>`__.

% CPU
-----

Cpu usage tracked during tp5 test runs. This metric is sampled every 20
seconds. This metric is collected on windows only.

Responsiveness
--------------

contact: :jimm, :overholt

Measures the delay for the event loop to process a `tracer
event <https://wiki.mozilla.org/Performance/Snappy#Current_Infrastructure>`__.
For more details, see `bug
631571 <https://bugzilla.mozilla.org/show_bug.cgi?id=631571>`__.

The score on this benchmark is proportional to the sum of squares of all
event delays that exceed a 20ms threshold. Lower is better.

We collect 8000+ data points from the browser during the test and apply
`this
formula <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/output.py#l95>`__
to the results:

.. code-block:: python

   return sum([float(x)*float(x) / 1000000.0 for x in val_list])

tpaint
======

.. warning::

   This test no longer exists

-  contact: :davidb
-  source:
   `tpaint-window.html <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/startup_test/tpaint.html>`__
-  type: Startup_
-  data: we load the tpaint test window 20 times, resulting in 1 set of
   20 data points.
-  summarization:

   -  subtest: `ignore first`_ **5** data points, then take the `median`_ of the remaining 15; `source:
      test.py <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/test.py#l190>`__
   -  suite: identical to subtest

+-----------------+---------------------------------------------------+
| Talos test name | Description                                       |
+-----------------+---------------------------------------------------+
| tpaint          | twinopen but measuring the time after we receive  |
|                 | the `MozAfterPaint and OnLoad event <#paint>`__.  |
+-----------------+---------------------------------------------------+

Tests the amount of time it takes the open a new window. This test does
not include startup time. Multiple test windows are opened in
succession, results reported are the average amount of time required to
create and display a window in the running instance of the browser.
(Measures ctrl-n performance.)

**Example Data**

.. code-block:: none

    [209.219, 222.180, 225.299, 225.970, 228.090, 229.450, 230.625, 236.315, 239.804, 242.795, 244.5, 244.770, 250.524, 251.785, 253.074, 255.349, 264.729, 266.014, 269.399, 326.190]

Possible regression causes
--------------------------

-  None listed yet. If you fix a regression for this test and have some
   tips to share, this is a good place for them.

xperf
=====

-  contact: fx-perf@mozilla.com
-  source: `xperf
   instrumentation <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/xtalos>`__
-  type: `Page load`_ (tp5n) / Startup_
-  measuring: IO counters from windows (currently, only startup IO is in
   scope)
-  reporting: Summary of read/write counters for disk, network (lower is
   better)

These tests only run on windows builds. See `this active-data
query <https://activedata.allizom.org/tools/query.html#query_id=zqlX+2Jn>`__
for an updated set of platforms that xperf can be found on. If the query
is not found, use the following on the query page:

.. code-block:: javascript

   {
       "from":"task",
       "groupby":["run.name","build.platform"],
       "limit":2000,
       "where":{"regex":{"run.name":".*xperf.*"}}
   }

Talos will turn orange for 'x' jobs on windows 7 if your changeset
accesses files which are not predefined in the
`allowlist <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/xtalos/xperf_allowlist.json>`__
during startup; specifically, before the
"`sessionstore-windows-restored <https://hg.mozilla.org/mozilla-central/file/0eebc33d8593/toolkit/components/startup/nsAppStartup.cpp#l631>`__"
Firefox event. If your job turns orange, you will see a list of files in
Treeherder (or in the log file) which have been accessed unexpectedly
(similar to this):

.. code-block:: none

    TEST-UNEXPECTED-FAIL : xperf: File '{profile}\secmod.db' was accessed and we were not expecting it. DiskReadCount: 6, DiskWriteCount: 0, DiskReadBytes: 16904, DiskWriteBytes: 0
    TEST-UNEXPECTED-FAIL : xperf: File '{profile}\cert8.db' was accessed and we were not expecting it. DiskReadCount: 4, DiskWriteCount: 0, DiskReadBytes: 33288, DiskWriteBytes: 0
    TEST-UNEXPECTED-FAIL : xperf: File 'c:\$logfile' was accessed and we were not expecting it. DiskReadCount: 0, DiskWriteCount: 2, DiskReadBytes: 0, DiskWriteBytes: 32768
    TEST-UNEXPECTED-FAIL : xperf: File '{profile}\secmod.db' was accessed and we were not expecting it. DiskReadCount: 6, DiskWriteCount: 0, DiskReadBytes: 16904, DiskWriteBytes: 0
    TEST-UNEXPECTED-FAIL : xperf: File '{profile}\cert8.db' was accessed and we were not expecting it. DiskReadCount: 4, DiskWriteCount: 0, DiskReadBytes: 33288, DiskWriteBytes: 0
    TEST-UNEXPECTED-FAIL : xperf: File 'c:\$logfile' was accessed and we were not expecting it. DiskReadCount: 0, DiskWriteCount: 2, DiskReadBytes: 0, DiskWriteBytes: 32768

In the case that these files are expected to be accessed during startup
by your changeset, then we can add them to the
`allowlist <https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Talos>`__.

Xperf runs tp5 while collecting xperf metrics for disk IO and network
IO. The providers we listen for are:

-  `'PROC_THREAD', 'LOADER', 'HARD_FAULTS', 'FILENAME', 'FILE_IO',
   'FILE_IO_INIT' <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/xperf.config#l10>`__

The values we collect during stackwalk are:

-  `'FileRead', 'FileWrite',
   'FileFlush' <https://dxr.mozilla.org/mozilla-central/source/testing/talos/talos/xperf.config#l11>`__

Notes:

-  Currently some runs may `return all-zeros and skew the
   results <https://bugzilla.mozilla.org/show_bug.cgi?id=1614805>`__
-  Additionally, these runs don't have dedicated hardware and have a
   large variability. At least 30 runs are likely to be needed to get
   stable statistics (xref `bug
   1616236 <https://bugzilla.mozilla.org/show_bug.cgi?id=1616236>`__)

Build metrics
*************

These are not part of the Talos code, but like Talos they are benchmarks
that record data using the graphserver and are analyzed by the same
scripts for regressions.

Number of constructors (num_ctors)
==================================

This test runs at build time and measures the number of static
initializers in the compiled code. Reducing this number is helpful for
`startup
optimizations <https://blog.mozilla.org/tglek/2010/05/27/startup-backward-constructors/>`__.

-  https://hg.mozilla.org/build/tools/file/348853aee492/buildfarm/utils/count_ctors.py

   -  these are run for linux 32+64 opt and pgo builds.

Platform microbenchmark
***********************

IsASCII and IsUTF8 gtest microbenchmarks
========================================

-  contact: :hsivonen
-  source:
   `TestStrings.cpp <https://dxr.mozilla.org/mozilla-central/source/xpcom/tests/gtest/TestStrings.cpp>`__
-  type: Microbench_
-  reporting: intervals in ms (lower is better)
-  data: each test is run and measured 5 times
-  summarization: take the `median`_ of the 5 data points; `source:
   MozGTestBench.cpp <https://dxr.mozilla.org/mozilla-central/source/testing/gtest/mozilla/MozGTestBench.cpp#43-46>`__

Test whose name starts with PerfIsASCII test the performance of the
XPCOM string IsASCII function with ASCII inputs if different lengths.

Test whose name starts with PerfIsUTF8 test the performance of the XPCOM
string IsUTF8 function with ASCII inputs if different lengths.

Possible regression causes
--------------------------

-  The --enable-rust-simd accidentally getting turned off in automation.
-  Changes to encoding_rs internals.
-  LLVM optimizations regressing between updates to the copy of LLVM
   included in the Rust compiler.

Microbench
==========

-  contact: :bholley
-  source:
   `MozGTestBench.cpp <https://dxr.mozilla.org/mozilla-central/source/testing/gtest/mozilla/MozGTestBench.cpp>`__
-  type: Custom GTest micro-benchmarking
-  data: Time taken for a GTest function to execute
-  summarization: Not a Talos test. This suite is provides a way to add
   low level platform performance regression tests for things that are
   not suited to be tested by Talos.

PerfStrip Tests
===============

-  contact: :davidb
-  source:
   https://dxr.mozilla.org/mozilla-central/source/xpcom/tests/gtest/TestStrings.cpp
-  type: Microbench_
-  reporting: execution time in ms (lower is better) for 100k function
   calls
-  data: each test run and measured 5 times
-  summarization:

PerfStripWhitespace - call StripWhitespace() on 5 different test cases
20k times (each)

PerfStripCharsWhitespace - call StripChars("\f\t\r\n") on 5 different
test cases 20k times (each)

PerfStripCRLF - call StripCRLF() on 5 different test cases 20k times
(each)

PerfStripCharsCRLF() - call StripChars("\r\n") on 5 different test cases
20k times (each)

Stylo gtest microbenchmarks
===========================

-  contact: :bholley, :SimonSapin
-  source:
   `gtest <https://dxr.mozilla.org/mozilla-central/source/layout/style/test/gtest>`__
-  type: Microbench_
-  reporting: intervals in ms (lower is better)
-  data: each test is run and measured 5 times
-  summarization: take the `median`_ of the 5 data points; `source:
   MozGTestBench.cpp <https://dxr.mozilla.org/mozilla-central/source/testing/gtest/mozilla/MozGTestBench.cpp#43-46>`__

Servo_StyleSheet_FromUTF8Bytes_Bench parses a sample stylesheet 20 times
with Stylo’s CSS parser that is written in Rust. It starts from an
in-memory UTF-8 string, so that I/O or UTF-16-to-UTF-8 conversion is not
measured.

Gecko_nsCSSParser_ParseSheet_Bench does the same with Gecko’s previous
CSS parser that is written in C++, for comparison.

Servo_DeclarationBlock_SetPropertyById_Bench parses the string "10px"
with Stylo’s CSS parser and sets it as the value of a property in a
declaration block, a million times. This is similar to animations that
are based on JavaScript code modifying Element.style instead of using
CSS @keyframes.

Servo_DeclarationBlock_SetPropertyById_WithInitialSpace_Bench is the
same, but with the string " 10px" with an initial space. That initial
space is less typical of JS animations, but is almost always there in
stylesheets or full declarations like "width: 10px". This microbenchmark
was used to test the effect of some specific code changes. Regressions
here may be acceptable if Servo_StyleSheet_FromUTF8Bytes_Bench is not
affected.

History of tp tests
*******************

tp
==

The original tp test created by Mozilla to test browser page load time.
Cycled through 40 pages. The pages were copied from the live web during
November, 2000. Pages were cycled by loading them within the main
browser window from a script that lived in content.

tp2/tp_js
=========

The same tp test but loading the individual pages into a frame instead
of the main browser window. Still used the old 40 page, year 2000 web
page test set.

tp3
===

An update to both the page set and the method by which pages are cycled.
The page set is now 393 pages from December, 2006. The pageloader is
re-built as an extension that is pre-loaded into the browser
chrome/components directories.

tp4
===

Updated web page test set to 100 pages from February 2009.

tp4m
====

This is a smaller pageset (21 pages) designed for mobile Firefox. This
is a blend of regular and mobile friendly pages.

We landed on this on April 18th, 2011 in `bug
648307 <https://bugzilla.mozilla.org/show_bug.cgi?id=648307>`__. This
runs for Android and Maemo mobile builds only.

tp5
===

Updated web page test set to 100 pages from April 8th, 2011. Effort was
made for the pages to no longer be splash screens/login pages/home pages
but to be pages that better reflect the actual content of the site in
question. There are two test page data sets for tp5 which are used in
multiple tests (i.e. awsy, xperf, etc.): (i) an optimized data set
called tp5o, and (ii) the standard data set called tp5n.

tp6
===

Created June 2017 with recorded pages via mitmproxy using modern google,
amazon, youtube, and facebook. Ideally this will contain more realistic
user accounts that have full content, in addition we would have more
than 4 sites- up to top 10 or maybe top 20.

These were migrated to Raptor between 2018 and 2019.

.. _geometric mean: https://wiki.mozilla.org/TestEngineering/Performance/Talos/Data#geometric_mean
.. _ignore first: https://wiki.mozilla.org/TestEngineering/Performance/Talos/Data#ignore_first
.. _median: https://wiki.mozilla.org/TestEngineering/Performance/Talos/Data#median
