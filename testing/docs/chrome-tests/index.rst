Chrome Tests
============

.. _DISCLAIMER:

**DISCLAIMER**
~~~~~~~~~~~~~~

**NOTE: Please use this document as a reference for existing chrome tests as you do not want to create new chrome tests.
If you're trying to test privileged browser code, write a browser mochitest instead;
if you are testing web platform code, use a wpt test, or a "plain" mochitest if you are unable to use a wpt test.**

.. _Introduction:

Introduction
~~~~~~~~~~~~

A chrome test is similar but not equivalent to a Mochitest running with chrome privileges.

The chrome test suite is an automated testing framework designed to
allow testing of application chrome windows using JavaScript.
It allows you to run JavaScript code in the non-electroysis (e10s) content area
with chrome privileges, instead of directly in the browser window (as browser tests do instead).
These tests reports results using the same functions as the Mochitest test framework.
The chrome test suite depends on runtests.py from the Mochitest framework.

.. _Running_the_chrome_tests:

Running the chrome tests
~~~~~~~~~~~~~~~~~~~~~~~~

To run chrome tests, you need to `build
Firefox </setup>`__ with your
changes and find the test or test manifest you want to run.

For example, to run all chrome tests under `toolkit/content`, run the following command:

::

   ./mach test toolkit/content/test/chrome/chrome.ini

To run a single test, just pass the path to the test into mach:

::

   ./mach test toolkit/content/tests/chrome/test_largemenu.html

You can also pass the path to a directory containing many tests. Run
`./mach test --help` for full documentation.

.. _Writing_chrome_tests:

Writing chrome tests
~~~~~~~~~~~~~~~~~~~~

A chrome tests is similar but not equivalent to a Mochitest
running with chrome privileges, i.e. code and UI are referenced by
``chrome://`` URIs. A basic XHTML test file could look like this:

.. code:: xml

   <?xml version="1.0"?>
   <?xml-stylesheet href="chrome://global/skin" type="text/css"?>
   <?xml-stylesheet href="chrome://mochikit/content/tests/SimpleTest/test.css" type="text/css"?>

   <window title="Demo Test"
           xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">
     <title>Demo Test</title>

     <script type="application/javascript"
             src="chrome://mochikit/content/tests/SimpleTest/SimpleTest.js"/>

     <script type="application/javascript">
     <![CDATA[
       add_task(async function runTest() {
          ok  (true ==  1, "this passes");
         todo(true === 1, "this fails");
       });
     ]]>
     </script>

     <body xmlns="http://www.w3.org/1999/xhtml">
       <p id="display"></p>
       <div id="content" style="display: none"></div>
       <pre id="test"></pre>
     </body>
   </window>


The comparison functions are identical to those supported by Mochitests,
see how the comparison functions work
in the Mochitest documentation for more details. The `EventUtils helper
functions <https://searchfox.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/EventUtils.js>`__
are available on the "EventUtils" object defined in the global scope.

The test suite also supports asynchronous tests.
To use these asynchronous tests, please use the `add_task() <https://searchfox.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/SimpleTest.js#2025>`__ functionality.

Any exceptions thrown while running a test will be caught and reported
in the test output as a failure. Exceptions thrown outside of the test's
context (e.g. in a timeout, event handler, etc) will not be caught, but
will result in a timed out test.

The test file name must be prefixed with "test_", and must have a file
extension of ".xhtml". Files that don't match this pattern will be ignored
by the test harness, but you still can include them. For example, a XUL
window file opened by your test_demo.xhtml via openDialog should be named
window_demo.xhtml. Putting the bug number in the file name is recommended
if your test verifies a bugfix, e.g. "test_bug123456.xhtml".

Helper files can be included, for example, from
``https://example.com/chrome/dom/workers/test/serviceworkers/serviceworkermanager_iframe.html``.

.. _Adding_a_new_chrome_test_to_the_tree:

Adding a new chrome test to the tree
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add a new chrome test to the tree, please use `./mach test addtest the_test_directory/the_test_you_want_to_create.xhtml`.
For more information about `addtest`, please run `./mach test addtest --help`.
