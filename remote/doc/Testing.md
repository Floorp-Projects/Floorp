Testing
=======

xpcshell unit tests
-------------------

The remote agent has a set of [xpcshell] unit tests located in
_remote/test/unit_.  These can be run this way:

	% ./mach test remote/test/unit

Because tests are run in parallel and xpcshell itself is quite
chatty, it can sometimes be useful to run the tests in sequence:

	% ./mach test --sequential remote/test/unit/test_Assert.js

The unit tests will appear as part of the `X` jobs on Treeherder.

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests
