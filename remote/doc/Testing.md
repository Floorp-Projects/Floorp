Testing
=======

The remote agent has unit- and functional tests located under
`remote/test/{unit,browser}`.

You may run all the tests under a particular subfolder like this:

	% ./mach test remote


Unit tests
----------

Because tests are run in parallel and [xpcshell] itself is quite
chatty, it can sometimes be useful to run the tests in sequence:

	% ./mach xcpshell-test --sequential remote/test/unit/test_Assert.js

The unit tests will appear as part of the `X` (for _xpcshell_) jobs
on Treeherder.

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests


Browser chrome tests
--------------------

We also have a set of functional [browser chrome] tests located
under _remote/test/browser_:

	% ./mach mochitest remote/test/browser/browser_cdp.js

The functional tests will appear under the `M` (for _mochitest_)
category in the `bc` (_browser-chrome_) jobs on Treeherder.

As the functional tests will sporadically pop up new Firefox
application windows, a helpful tip is to run them in [headless
mode]:

	% ./mach mochitest --headless remote/test/browser

The `--headless` flag is equivalent to setting the `MOZ_HEADLESS`
environment variable.  You can additionally use `MOZ_HEADLESS_WIDTH`
and `MOZ_HEADLESS_HEIGHT` to control the dimensions of the virtual
display.

[browser chrome]: https://developer.mozilla.org/en-US/docs/Mozilla/Browser_chrome_tests
[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode


Puppeteer tests
---------------

In addition to our own Firefox-specific tests, we run the upstream
[Puppeteer test suite] against our implementation to track progress
towards achieving full [Puppeteer support] in Firefox.

These tests are vendored under _remote/test/puppeteer/_ and are
run locally like this:

	% ./mach test remote/test/puppeteer/test

On try they appear under the `remote(pup)` symbol, but because they’re
a Tier-3 class test job they’re not automatically scheduled.
To schedule the tests, look for `source-test-remote-puppeteer` in
`./mach try fuzzy`.

[Puppeteer test suite]: https://github.com/GoogleChrome/puppeteer/tree/master/test
[Puppeteer support]: https://bugzilla.mozilla.org/show_bug.cgi?id=puppeteer
