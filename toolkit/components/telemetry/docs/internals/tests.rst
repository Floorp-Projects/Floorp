Tests
=====

Firefox Telemetry is a complicated and old component.
So too are the organization and expanse of its tests.
Let’s break them down by harness.

Unless otherwise mentioned the tests live in subdirectories of
``toolkit/components/telemetry/tests``.

Mochitest
---------
:Location: ``t/c/t/t/browser/``
:Language: Javascript
  (`mochitest <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Mochitest>`_)

This test harness runs nearly the entire Firefox and gives access to multiple tabs and browser chrome APIs.
It requires window focus to complete correctly,
so it isn’t recommended to add new tests here.
The tests that are here maybe would be more at home as telemetry-tests-client tests as they tend to be integration tests.

Google Test
-----------
:Location: ``t/c/t/t/gtest/``
:Language: C++
  (`googletest <https://github.com/google/googletest>`_)

This test harness runs a specially-built gtest shell around libxul which allows you to write unit tests against public C++ APIs.
It should be used to test the C++ API and core of Firefox Telemetry.
This is for tests like
“Do we correctly accumulate to bucket 0 if I pass -1 to ``Telemetry::Accumulate``?”

Integration Tests (telemetry-tests-client and telemetry-integration-tests)
--------------------------------------------------------------------------
:Location: ``t/c/t/t/marionette/tests/client`` and ``t/c/t/t/integration/``
:Language: Python
  (`unittest <https://docs.python.org/3/library/unittest.html>`_,
  `pytest <https://docs.pytest.org/en/latest/>`_)

The most modern of the test harnesses,
telemetry-integration-tests uses marionette to puppet the entire browser allowing us to write integration tests that include ping servers and multiple browser runs.
You should use this if you’re testing Big Picture things like
“Does Firefox resend its “deletion-request” ping if the network is down when Telemetry is first disabled?”.

At time of writing there are two “editions” of integration tests.
Prefer writing new tests in telemetry-tests-client
(the unittest-based one in ``t/c/t/t/marionette/tests/client``)
while we evaluate CI support for telemetry-integration-tests.

Definitions Files Tests
-----------------------
:Location: ``t/c/t/t/python``
:Language: Python
  (`unittest <https://docs.python.org/3/library/unittest.html>`_)

This harness pulls in the parsers and scripts used to turn JSON and YAML probe definitions into code.
It should be used to test the build scripts and formats of the definitions files
Histograms.json, Scalars.yaml, and Events.yaml.
This is for tests like
“Does the build fail if someone forgot to put in a bug number for a new Histogram?”.

xpcshell
--------
:Location: ``t/c/t/t/unit``
:Language: Javascript
  (`xpcshell <https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests>`_)

This test harness uses a stripped-down shell of the Firefox browser to run privileged Javascript.
It should be used to write unit tests for the Javascript API and app-level logic of Firefox Telemetry.
This is for tests like
“Do we correctly accumulate to bucket 0 if I pass -1 to ``Telemetry.getHistogramById(...).add``?”
and
“Do we reschedule pings that want to be sent near local midnight?”.

Since these tests are easy to write and quick to run we have in the past bent this harness in a few interesting shapes
(see PingServer)
to have it support integration tests as well.
New integration tests should use telemetry-tests-client instead.

Instrumentation Tests
---------------------
:Location: Various
:Language: Usually Javascript
  (`xpcshell <https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests>`_ or
  `mochitest <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Mochitest>`_)

In addition to the tests of Firefox Telemetry,
other code owners have written tests that ensure that their code records appropriate values to Telemetry.
They should use the
``toolkit/components/telemetry/tests/unit/TelemetryTestUtils.jsm``
module to make their lives easier.
This can be used for tests like
“If five bookmarks are read from the database,
does the bookmark count Histogram have a value of 5 in it?”.

