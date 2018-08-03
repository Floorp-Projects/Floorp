.. -*- Mode: rst; fill-column: 80; -*-

========================================================
 Collecting code coverage information for Android tests
========================================================

The Android-specific test suites are outlined on MDN_. A more elaborate description can be found on
the Wiki_. This page describes how collecting code coverage information is implemented for these
test suites.

.. _MDN: https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites
.. _WIKI: https://wiki.mozilla.org/Mobile/Fennec/Android/Testing

Collecting and exporting code coverage information
==================================================

Relevant tools and libraries
----------------------------

JaCoCo_ is the tool used to gather code coverage data. JaCoCo uses class file
instrumentation to record execution coverage data. It has two operating modes:

- `online instrumentation`_: Class files are instrumented on-the-fly using a so
  called Java agent. The JaCoCo agent collects execution information and dumps
  it on request or when the JVM exits. This method is used for test suites
  which run on the JVM (generally, `*UnitTest` Gradle tasks).
- `offline instrumentation`_: At runtime the pre-instrumented classes needs be
  on the classpath instead of the original classes. In addition,
  `jacocoagent.jar` must be put on the classpath. This method is used for test
  suites with run on the Android emulator (generally, `*AndroidTest` Gradle
  tasks, including `robocop`).

JaCoCo is integrated with Gradle in two ways: a Gradle plugin (activated in the
``build.gradle`` file as a top-level ``apply plugin: 'jacoco'``), and an
Android-specific Gradle plugin, activated with ``android { buildTypes { debug {
testCoverageEnabled true } } }``. These two methods of activating JaCoCo have a
different syntax of choosing the JaCoCo tool version, and both should be
configured to use the same version. See the
``mobile/android/geckoview/build.gradle`` file for example usage.

grcov_ is a tool implemented in Rust that transforms code coverage reports
between formats. Among others, it supports reading JaCoCo XML reports.

.. _JaCoCo: https://www.eclemma.org/jacoco/
.. _online instrumentation: https://www.jacoco.org/jacoco/trunk/doc/agent.html
.. _offline instrumentation: https://www.jacoco.org/jacoco/trunk/doc/offline.html
.. _grcov: https://github.com/mozilla/grcov/

Generating the coverage report artifacts
----------------------------------------

All tasks that output code coverage information do so by exporting an artifact
named ``code-coverage-grcov.zip`` which contains a single file named
``grcov_lcov_output.info``. The ``grcov_lcov_output.info`` file should contain
coverage information in the lcov format. The artifact, once uploaded, is picked
up and indexed by ActiveData_.

The code that generates the ``code-coverage-grcov.zip`` artifact after a
generally resides in the ``CodeCoverageMixin`` class, in the module
``testing/mozharness/mozharness/mozilla/testing/codecoverage.py``. This class is
responsible for downloading ``grcov`` along with other artifacts from the build
job. It is also responsible for running ``grcov`` after the tests are finished,
to convert and merge the coverage reports.

.. _ActiveData: https://wiki.mozilla.org/EngineeringProductivity/Projects/ActiveData

Code coverage for android-test
===============================

The `android-test` suite is a JUnit test suite that runs locally on the
host's JVM. It can be run with ``mach android test``. The test suite is
implemented as a build task, defined at
``taskcluster/ci/build/android-stuff.yml``.

To collect code coverage from this suite, a duplicate build task is defined,
called `android-test-ccov`. It can be run with ``mach android test-ccov``.

The mach subcommand is responsible for downloading and running `grcov`,
instead of the ``CodeCoverageMixin`` class. This is because the
`android-test-ccov` task is a build task (not a test task), so it doesn't use
mozharness to run the tests.


Code coverage for geckoview-junit
==================================

The geckoview-junit_ tests are on-device Android JUnit tests written for
GeckoView_. The tests are implemented with ``mochitest``. The automation
entry point is ``testing/mozharness/scripts/android_emulator_unittest.py``,
which then calls ``testing/mochitest/runjunit.py``. This is an out-of-tree
task, so a source tree clone is unavailable.

To generate the coverage report, we need three things:

- The classfiles before instrumentation. These are archived as a public
  artifact during build time, and downloaded on the test machine during testing
  time;
- The coverage.ec file with coverage counters. This is generated while running
  the tests on the emulator, then downloaded with ``adb pull``;
- ``jacoco-cli``, a command-line package JaCoCo component that takes the
  classfiles and the coverage counters as input and generates XML reports as
  output.

The ``mach android archive-geckoview-coverage-artifacts`` command archives the
class files and exports the ``jacoco-cli`` jar file after the build is done.
These files are later saved as public artifacts of the build.

To enable offline instrumentation for the test suites, the mozconfig flag
``--enable-java-coverage`` should be set. When the flag is checked both during
build and test time. During test time, the flag instructs ``CodeCoverageMixin``
to download the coverage artifacts from the build task before the tests run,
and generate and export the reports after testing is finished. The flag also
instructs the ``runjunit.py`` script to insert the arguments ``-e coverage
true`` to ``am instrument``.

.. _GeckoView: https://wiki.mozilla.org/Mobile/GeckoView
.. _geckoview-junit: https://developer.mozilla.org/en-US/docs/Mozilla/Geckoview-Junit_Tests
