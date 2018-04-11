To run the tests in this directory, from the top source directory,
either invoke the test despatcher in mach:

	% ./mach test testing/marionette/test/unit

Or call out the harness specifically:

	% ./mach xpcshell-test testing/marionette/test/unit

The latter gives you the `--sequential` option which can be useful
when debugging to prevent tests from running in parallel.

When adding new tests you must make sure they are listed in
xpcshell.ini, otherwise they will not run on try.
