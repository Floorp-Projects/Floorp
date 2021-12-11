=================
Integration Tests
=================

The aim of the telemetry-tests-client suite is to verify Firefox collects telemetry probes, aggregates that data, and submits telemetry
pings containing the data to a HTTP server. The integration tests try to make no assumptions about the internal workings of Firefox and
use automation to mimic user behavior.

The integration test suite for Firefox Client Telemetry runs on CI `tier 1 <https://wiki.mozilla.org/Sheriffing/Job_Visibility_Policy>`_
with treeherder symbol `tt(c)`
and is checked in to version control at mozilla-central under
`toolkit/components/telemetry/tests/marionette/tests/client <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/tests/marionette/tests/client/>`_.

Test Main Tab Scalars
---------------------

- PATH: ``telemetry/tests/marionette/tests/client/test_main_tab_scalars.py``
- This test opens and closes a number of browser tabs,
  restarts the browser in a new session
  and then verifies the correctness of scalar data in the resulting `main` ping.

Test Search Counts
------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_search_counts_across_subsessions.py``
- This test performs a search in a new tab,
  restarts Firefox in a new session and verifies the correctness of client, session and subsession IDs,
  as well as scalar and keyed histogram data in the `shutdown` ping,
  installs an addon, verifies the `environment-change` ping, and performs three additional search actions
  before restarting and verifying the new `main` ping.


Test Deletion Request Ping
--------------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_deletion_request_ping.py``
- This test installs an addon and verifies a ping is received. The test takes note of the client ID.
  It then disables telemetry and checks for a `deletion-request` ping.
  After it receives the correct ping it makes sure that no other pings are sent.
  Telemetry is then re-enabled and the `main` ping is checked to see if the client ID has changed.
  The test asserts that the user has opted back in to telemetry.

Test Event Ping
---------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_event_ping.py``
- This test checks for a basic `event` ping. It opens firefox, performs a search and checks the `event`
  ping for the correct number of searches performed (1) and the correct search engine.

Test Fog Custom Ping
--------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_fog_custom_ping.py``
- This test creates a custom ping using the Glean API and asserts this ping is sent correctly.

Test Fog Deletion Request Ping
------------------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_fog_deletion_request_ping.py``
- This test opens the browser, performs a search and disables telemetry after the search.
  It asserts that the telemetry is disabled and no pings exist.
  The browser is restarted and telemetry is then re-enabled.
  Then we set a `debug tag <https://mozilla.github.io/glean/book/user/debugging/debug-ping-view.html>`_
  which is attached to the ping.
  Telemetry is then disabled again to trigger a `deletion-request` ping.
  We verify that 1) The debug tag is present; and 2) that the client ID
  in the second `deletion-request` ping is different from the first client ID.

Test Fog User Activity
----------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_fog_user_activity.py``
- This test checks that a `baseline` ping is sent when the user starts or stops using Firefox.

Test Background Update Ping
---------------------------

- PATH: ``toolkit/telemetry/tests/marionette/tests/client/test_fog_user_activity.py``
- In this test we launch Firefox to prepare a profile and to disable the background update setting.
  We exit Firefox,
  leaving the (unlocked) profile to be used as the default profile for the background update task (and not having multiple instances running).
  The task will not try to update, but it will send a ping.
  Then we restart Firefox to unwind the background update setting and allow shutdown to proceed cleanly.

Running the tests locally
-------------------------

You can run the tests on your local machine using
`mach <https://firefox-source-docs.mozilla.org/mach/index.html>`__:

``./mach telemetry-tests-client``

Running the tests on try
------------------------

You can run the tests across all platforms on the try server using
`mach <https://firefox-source-docs.mozilla.org/mach/index.html>`__:

``./mach try fuzzy -q "'telemetry-tests-client"``

Disabling an individual failing test
------------------------------------

The telemetry-tests-client suite is implemented in Python and uses Marionette for browser automation and wptserve for the HTTP ping server.
The integration tests are based on Python's unittest testing library and can be disabled by calling
`self.skipTest("reason") <https://docs.python.org/3/library/unittest.html#skipping-tests-and-expected-failures>`_ in a test method.

The example below demonstrates how to disable test_main_ping2:

.. code-block:: python

  import unittest

  from telemetry_harness.testcase import TelemetryTestCase

  class TestMainPingExample(TelemetryTestCase):
      """Example tests for the telemetry main ping."""

      def test_main_ping1(self):
          """Example test that we want to run."""

          self.search_in_new_tab("mozilla firefox")

      def test_main_ping2(self):
          """Example test that we want to skip."""

          self.skipTest("demonstrating skipping")

          self.search_in_new_tab("firefox telemetry")


Who to contact for help
-----------------------

- The test cases are owned by Chris Hutten-Czapski (chutten on matrix) from the Firefox Telemetry team
  (`#telemetry <https://chat.mozilla.org/#/room/#telemetry:mozilla.org>`__ on matrix)
- The test harness is owned by Raphael Pierzina (raphael on matrix) from the Ecosystem Test Engineering team
  (`#telemetry <https://chat.mozilla.org/#/room/#telemetry:mozilla.org>`__ on matrix)

Bugzilla
--------

Bugs can be filed under the Toolkit product for the Telemetry component.
