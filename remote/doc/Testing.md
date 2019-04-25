Testing
=======

The remote agent has unit- and functional tests located under
`remote/test/{unit,browser}`.

You may run all the tests locally using `mach test` like this:

	% ./mach test remote/test

The tests are currently not run on try.


Unit tests
----------

Because tests are run in parallel and [xpcshell] itself is quite
chatty, it can sometimes be useful to run the tests in sequence:

	% ./mach xcpshell-test --sequential remote/test/unit/test_Assert.js

The unit tests will appear as part of the `X` jobs on Treeherder.

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests


Functional tests
----------------

We also have a set of functional [browser chrome] tests located
under _remote/test/browser_:

	% ./mach mochitest -f browser remote/test/browser/browser_cdp.js

[browser chrome]: https://developer.mozilla.org/en-US/docs/Mozilla/Browser_chrome_tests
