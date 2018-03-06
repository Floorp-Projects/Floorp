Running tests
=============

We verify and test Marionette in a couple of different ways.
Marionette has a set of [xpcshell] unit tests located in
_testing/marionette/test_*.js_.  These can be run this way:

	% ./mach test testing/marionette/test_*.js

Because tests are run in parallell and xpcshell itself is quite
chatty, it can sometimes be useful to run the tests sequentially:

	% ./mach test --sequential testing/marionette/test_error.js

These unit tests run as part of the _X_ jobs on Treeherder.

We also have a set of functional tests that make use of the Marionette
Python client.  These start a Firefox process and tests the Marionette
protocol input and output.  The following command will run all tests:

	% ./mach test mn

But you can also run individual tests:

	% ./mach test testing/marionette/harness/marionette_harness/tests/unit/test_navigation.py

When working on Marionette code it is often useful to surface the
stdout from Firefox:

	% ./mach test --gecko-log - TEST

It is common to use this in conjunction with an option to increase
the Marionette log level:

	% ./mach test --gecko-log - -vv TEST

A single `-v` enables debug logging, and a double `-vv` enables
trace logging.

As these are functional integration tests and pop up Firefox windows
sporadically, a helpful tip is to surpress the window whilst you
are running them by using Firefox’ [headless mode]:

	% ./mach test -z TEST

`-z` is an alias for `--headless` and equivalent to setting the
`MOZ_HEADLESS` output variable.  In addition to `MOZ_HEADLESS`
there is also `MOZ_HEADLESS_WIDTH` and `MOZ_HEADLESS_HEIGHT` for
controlling the dimensions of the no-op virtual display.  This is
similar to using xvfb(1) which you may know from the X windowing system,
but has the additional benefit of also working on macOS and Windows.

We have a separate page documenting how to write good Python tests in
<doc/PythonTests.md>.  These tests will run as part of the _Mn_
job on Treeherder.

In addition to these two test types that specifically test the
Marionette protocol, Marionette is used as the backend for the
[geckodriver] WebDriver implementation.  It is served by a WPT test
suite which effectively tests conformance to the W3C specification.

This is a good try syntax to use when testing Marionette changes:

	-b do -p linux,linux64,macosx64,win64,android-api-16 -u marionette-e10s,marionette-headless-e10s,xpcshell,web-platform-tests,firefox-ui-functional-local-e10s,firefox-ui-functional-remote-e10s -t none

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests
[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode
[geckodriver]: ../geckodriver/README.md
