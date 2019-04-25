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


Functional tests
----------------

We also have a set of functional [browser chrome] tests located
under _remote/test/browser_:

	% ./mach mochitest -f browser remote/test/browser/browser_cdp.js

The functional tests will appear under the `M` (for _mochitest_)
category in the `bc` (_browser-chrome_) jobs on Treeherder.

[browser chrome]: https://developer.mozilla.org/en-US/docs/Mozilla/Browser_chrome_tests
