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

grcov_ is a tool implemented in Rust that transforms code coverage reports
between formats. Among others, it supports reading JaCoCo XML reports.

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
