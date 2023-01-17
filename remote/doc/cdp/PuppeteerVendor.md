Vendoring Puppeteer
===================

As mentioned in the chapter on [Testing], we run the full [Puppeteer
test suite] on try.  These tests are vendored in central under
_remote/test/puppeteer/_ and we have a script to pull in upstream changes.

We periodically perform a manual two-way sync. Below is an outline of the
process interspersed with some tips.

1. Clone the Puppeteer git repository and checkout the release tag you want
   to vendor into mozilla-central.

    	% git checkout tags/v10.0 -b sync-v10.0

2. Apply any recent changes in `remote/test/puppeteer` to the Puppeteer branch
   created above.

	 You might want to [install the project] at this point and make sure unit
	 tests pass. Check the project's `package.json` for relevant testing commands.

   You should use this as basis for a PR to the Puppeteer project once you are
	 satisfied that the two-way sync will be successful in mozilla-central. See
	 their [contributing.md].

	 Typically, the changes we push to Puppeteer include unskipping newly passing
	 unit tests for Firefox along with minor fixes to the tests or
	 to Firefox-specific browser-fetching and launch code.

	 Be sure to [run tests against both Chromium and Firefox] in the Puppeteer
	 repo. You can specify your local Firefox build when you do so:

		% BINARY=<path-to-objdir-binary> npm run funit

3. Now back in mozilla-central, you can run the following mach command to
	 copy over the Puppeteer branch you just prepared. The mach command has
	 flags to specify a local or remote repository as well as a commit.

		% ./mach remote vendor-puppeteer

	 By default, this command also installs the newly-pulled Puppeteer package
	 in order to generate a new `package-lock.json` file for the purpose of
	 pinning Puppeteer dependencies for our CI. There is a `--no-install` option
	 if you want to skip this step; for example, if you want to run installation
	 separately at a later point.

4. Use `./mach puppeteer-test` (see [Testing]) to run Puppeteer tests against
   both Chromium and Firefox in headless mode. Again, only running a subset of
	 tests against Firefox is fine -- at this point you just want to check that
	 the typescript compiles and the browser binaries are launched successfully.

	 If something at this stage fails, you might want to check changes
	 in `remote/test/puppeteer/package.json` and update `remote/mach_commands.py`
	 with new npm scripts.

5. Next, you want to make sure that expectation meta data is correct.
	Check changes in `remote/test/puppeteer/test/TestExpectations.json`,
	if there are newly skipped tests for Firefox, you might need to update these expectations. To do this, run the Puppeteer test job on try (see [Testing]). If these tests are specific for Chrome or time out, we want to keep them skipped, if they fail we want to have `FAIL` status in the
	expectation meta data. You can see, if the meta data needs to be updated, at the end of the log file.

	Examine the job logs and makes sure the run didn't get interrupted early
	by a crash or a hang, especially if you see a lot of `TEST-UNEXPECTED-MISSING` in the Treeherder Failure Summary.
	You might need fix some new bug in the unit tests. This is the fun part.

	Some tests can also unexpectedly pass. Make sure it's correct, and if needed update the expectation data
	by following the instructions at the end of the log file.
6. Once you are happy with the metadata and are ready to submit the sync patch
   up for review, run the Puppeteer test job on try again with `--rebuild 10`
	 to check for stability.

[Testing]: ../Testing.md
[Puppeteer test suite]: https://github.com/GoogleChrome/puppeteer/tree/master/test
[install the project]: https://github.com/puppeteer/puppeteer/blob/main/docs/contributing.md#getting-code
[run tests against both Chromium and Firefox]: https://github.com/puppeteer/puppeteer/blob/main/test/README.md#running-tests
[puppeteer-expected.json]: https://searchfox.org/mozilla-central/source/remote/test/puppeteer-expected.json
[contributing.md]: https://github.com/puppeteer/puppeteer/blob/main/docs/contributing.md
[unskip]: https://github.com/puppeteer/puppeteer/blob/main/test/README.md#skipping-tests-in-specific-conditions
