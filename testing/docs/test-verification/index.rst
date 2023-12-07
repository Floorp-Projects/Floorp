Test Verification
=================

When a changeset adds a new test, or modifies an existing test, the test
verification (TV) test suite performs additional testing to help find
intermittent failures in the modified test as quickly as possible. TV
uses other test harnesses to run the test multiple times, sometimes in a
variety of configurations. For instance, when a mochitest is
modified, TV runs the mochitest harness in a verify mode on the modified
mochitest. That test will be run 10 times, then the same test will be
run another 5 times, each time in a new browser instance. Once this is
done, the whole sequence will be repeated in the test chaos mode
(setting MOZ_CHAOSMODE). If any test run fails then the failure is
reported normally, testing ends, and the test suite reports the failure.

Initially, there are some limitations:

-  TV only applies to mochitests (all flavors and subsuites), reftests
   (including crashtests and js-reftests) and xpcshell tests; a separate
   job, TVw, handles web-platform tests.
-  Only some of the test chaos mode features are enabled

.. _Running_test_verification_with_mach:

Running test verification with mach
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Supported test harnesses accept the --verify option:

::

   mach web-platform-test <test> --verify

   mach mochitest <test> --verify

   mach reftest <test> --verify

   mach xpcshell-test <test> --verify

Multiple tests, even manifests or directories, can be verified at once,
but this is generally not recommended. Verification is easier to
understand one test at a time!

.. _Verification_steps:

Verification steps
~~~~~~~~~~~~~~~~~~

Each test harness implements --verify behavior in one or more "steps".
Each step uses a different strategy for finding intermittent failures.
For instance, the first step in mochitest verification is running the
test with --repeat=20; the second step is running the test just once in
a separate browser session, closing the browser, and repeating that
sequence several times. If a failure is found in one step, later steps
are skipped.

.. _Verification_summary:

Verification summary
~~~~~~~~~~~~~~~~~~~~

Test verification can produce a lot of output, much of it is repetitive.
To help communicate what verification has been found, each test harness
prints a summary for each file which has been verified. With each
verification step, there is either a pass or fail status and an overall
verification status, such as:

::

   :::
   ::: Test verification summary for:
   :::
   ::: dom/base/test/test_data_uri.html
   :::
   ::: 1. Run each test 20 times in one browser. : FAIL
   ::: 2. Run each test 10 times in a new browser each time. : not run / incomplete
   :::
   ::: Test verification FAILED!
   :::

.. _Long-running_tests_and_verification_duration:

Long-running tests and verification duration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Test verification is intended to be quick: Determine if this test fails
intermittently as soon as possible, so that a pass or fail result is
communicated quickly and test resources are not wasted.

Tests have a wide range of run-times, from milliseconds up to many
minutes. Of course, a test that takes 5 minutes to run, may take a very
long time to verify. There may also be cases where many tests are being
verified at one time. For instance, in automation a changeset might make
a trivial change to hundreds of tests at once, or a merge might result
in a similar situation. Even if each test is reasonably quick to verify,
the time required to verify all these files may be considerable.

Each test harness which supports the --verify option also supports the
--max-verify-time option:

::

   mach mochitest <test> --verify --max-verify-time=7200

The default max-verify-time is 3600 seconds (1 hour). If a verification
step exceeds the max-verify-time, later steps are not run.

In automation, the TV task uses --max-verify-time to try to limit
verification to about 1 hour, regardless of how many tests are to be
verified or how long each one runs. If verification is incomplete, the
task does not fail. It reports success and is green in the treeherder,
in addition the treeherder "Job Status" pane will also report
"Verification too long! Not all tests were verified."

.. _Test_Verification_in_Automation:

Test Verification in Automation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In automation, the TV and TVw tasks run whenever a changeset contains
modifications to a .js, .html, .xhtml or .xul file. The TV/TVw task
itself checks test manifests to determine if any of the modified files
are test files; if any of the files are tests, TV/TVw will verify those
tests.

Treeherder status is:

-  **Green**: All modified tests in supported suites were verified with
   no test failures, or test verification did not have enough time to
   verify one or more tests.
-  **Orange**: One or more tests modified by this changeset failed
   verification. **Backout should be considered (but is not
   mandatory)**, to avoid future intermittent failures in these tests.

There are some limitations:

-  Pre-existing conditions: A test may be failing, then updated on a
   push in a net-positive way, but continue failing intermittently. If
   the author is aware of the remaining issues, it is probably best not
   to backout.
-  Failures due to test-verify conditions: In some cases, a test may
   fail because test-verify runs a test with --repeat, or because
   test-verify uses chaos mode, but those failures might not arise in
   "normal" runs of the test. Ideally, all tests should be able to run
   successfully in test-verify, but there may be exceptions.

.. _Test_Verification_on_try:

Test Verification on try
~~~~~~~~~~~~~~~~~~~~~~~~

To use test verification on try, use something like:

::

   mach try -b do -p linux64 -u test-verify-e10s --artifact

Tests modified in the push will be verified.

For TVw, use something like:

::

   mach try -b do -p linux64 -u test-verify-wpt-e10s --artifact

Web-platform tests modified in the push will be verified.

You can also run test verification on a test without modifying the test
using something like:

::

   mach try fuzzy <path-to-test>


.. _Skipping_Verification:

Skipping Verification
~~~~~~~~~~~~~~~~~~~~~

In the great majority of cases, test-verify failures indicate test
weaknesses that should be addressed.

In unusual cases, where test-verify failures does not provide value,
test-verify may be "skipped" on a test: In subsequent pushes where the
test is modified, the test-verify job will not try to verify the skipped
test.

For mochitests, xpcshell tests, and other tests using the .ini manifest
format, use something like:

::

   [sometest.html]
   skip-if = verify

For reftests (including crashtests and jsreftests), use something like:

::

   skip-if(verify) == sometest.html ...

At this time, there is no corresponding support for skipping
web-platform tests in verify mode.

.. _FAQ:

FAQ
~~~

**Why is there a "spike" of test-verify failures for my test? Why did it
stop?**

Bug reports for test-verify failures usually show a "spike" of failures
on one day. That's because TV only runs against a particular test when
that test is modified. A particular push modifies the test, TV runs and
the test fails, and then TV doesn't run again for that test on
subsequent pushes (until/unless the test files are modified again). Of
course, when that push is merged to other trees, TV is triggered again,
so the same failure is usually noted on multiple trees in succession:
say, one revision on mozilla-inbound, then again when that revision is
merged to mozilla-central and again on autoland.

**When TV fails, is it worth retriggering?**

No - usually not. TV runs specific tests over and over again - sometimes
50 times in a single run. Retriggering on treeherder is generally
unnecessary and will very likely produce the same pass/fail result as
the original run. In this sense, TV failures are almost always
"perma-fail".

.. _Contact_information:

Contact information
~~~~~~~~~~~~~~~~~~~

Test verification is maintained by :**gbrown** and :**jmaher**. Bugs
should be filed in **Testing :: General**. You may want to reference
`bug 1357513 <https://bugzilla.mozilla.org/show_bug.cgi?id=1357513>`__.
