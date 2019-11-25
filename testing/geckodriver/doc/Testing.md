Testing geckodriver
===================

We verify and test geckodriver in a couple of different ways.
Since it is an implementation of the WebDriver web standard, we share
a set of conformance tests with other browser vendors through the
[Web Platform Tests] (WPT) initiative.  This lets us ensure web
compatibility between _different_ WebDriver implementations for
different browsers.

In addition to the WPT tests, geckodriver and webdriver have
unit tests.  These are written in Rust, but you must explicitly
tell mach to build these by adding the following line to your [mozconfig]:

	ac_add_options --enable-rust-tests

Tests can then be run like this:

	% ./mach test testing/geckodriver

To run the more extensive WPT tests you can use mach, but first
make sure you have built Firefox:

	% ./mach build
	% ./mach wpt testing/web-platform/tests/webdriver

As these are functional integration tests and pop up Firefox windows
sporadically, a helpful tip is to suppress the window whilst you
are running them by using Firefox’ [headless mode]:

	% ./mach wpt --headless testing/web-platform/tests/webdriver

The `--headless` flag is equivalent to setting the `MOZ_HEADLESS`
output variable.  In addition to `MOZ_HEADLESS` there is also
`MOZ_HEADLESS_WIDTH` and `MOZ_HEADLESS_HEIGHT` for controlling the
dimensions of the no-op virtual display.  This is similar to using
Xvfb(1) which you may know from the X windowing system, but has
the additional benefit of also working on macOS and Windows.

As you get in to development of geckodriver and Marionette you will
increasingly grow to understand our love for [trace-level logs].
They provide us with the input—the HTTP requests—from the client
(in WPT’s case from the tests’ use of a custom WebDriver client),
the translation geckodriver makes to the [Marionette protocol],
the log output from Marionette, its responses back to geckodriver,
and finally the output—or the HTTP response—back to the client.

The [trace-level logs] can be surfaced by passing on the `-vv`
flag to geckodriver through WPT:

	% ./mach wpt --webdriver-arg=-vv testing/web-platform/tests/webdriver

[Web Platform Tests]: http://web-platform-tests.org/
[cargo]: http://doc.crates.io/guide.html
[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode
[mozconfig]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options
[trace-level logs]: TraceLogs.html
[Marionette protocol]: https://firefox-source-docs.mozilla.org/testing/marionette/Protocol.html
