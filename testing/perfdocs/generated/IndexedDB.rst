=============================
IndexedDB Performance Testing
=============================

How to run tests on CI:
-----------------------
* Windows: ``mach try perf --show-all -q "test-windows10-64-shippable-qr/opt-browsertime-indexeddb"``
* Linux: ``mach try perf --show-all -q "test-linux1804-64-shippable-qr/opt-browsertime-indexeddb"``
* Mac: ``mach try perf --show-all -q "test-macosx1015-64-shippable-qr/opt-browsertime-indexeddb"``
* All but 32-bit jobs: ``mach try perf --chrome --safari --show-all -q 'shippable-browsertime-indexeddb !32'``
* In general:

  * Open test selection interface with ``mach try perf --show-all``
  * Filter out the preferred tests by typing letters which are expected to be part of the test job name string (as in the -q argument above)
  * Note down the string used as a filter for rerunning the job (or rerun it with ``mach try again --list-tasks`` and ``mach try again --index``)

How to run tests locally with the profiler?
-------------------------------------------
* Build the browser with release or release with debug symbols flags (not in debug mode)
* Use ``mach raptor --browsertime -t $(test_name) --gecko-profile --post-startup-delay=1000`` where test name, such as ``addMarN`` is one of the items listed in ``testing/raptor/raptor/tests/custom/browsertime-indexeddb.ini``
* After the test is complete, the generated profile is opened with the default browser.
* The generated profile file path is listed also in the command line output.
* For best symbolication results, it may help to

  * run the same browser build that was used for the tests with ``./mach run``
  * navigate to "profiler.firefox.com"
  * use the "Load a profile from file" button

How to compare performance to a different browser?
--------------------------------------------------
* The test outputs a ``time_duration`` value for all supported browsers
* Using Chrome as an example,

  * ``mach raptor --browsertime -t $(test_name) --post-startup-delay=1000 --app=chrome -b "/c/Program Files/Google/Chrome/Application/chrome.exe"``
  * where test name, such as ``addMarN`` is one of the items listed in ``testing/raptor/raptor/tests/custom/browsertime-indexeddb.ini``
  * browser executable path after the ``-b`` argument varies locally
  * in some cases, a test driver argument such as ``--browsertime-chromedriver`` may be required

How to add more tests?
----------------------
* For the test boilerplate, copy and rename an old test script such as ``testing/raptor/browsertime/indexeddb_write.js`` under the ``testing/raptor/browsertime/`` directory
* Modify the test case script argument of ``commands.js.run`` / Selenium's ``executeAsyncScript``

  * Test parameters can be passed to such script with syntax ``${variable_name}`` where ``variable_name`` represents the parameter in the context of ``executeAsyncScript`` or ``commands.js.run``.
  * Use quotes to capture a string value, for example ``"${variable_name}"``
  * TIP: Debugging the test case could be simpler by serving it locally without the boilerplate

* Add ``[test_name]`` section to file ``testing/raptor/raptor/tests/custom/browsertime-indexeddb.ini`` where ``test_name`` **must be 10 characters or less** in order to be a valid ``Treeherder`` test name

  * Under the ``[test_name]`` section, specify the test script name as a value of ``test_script =``
  * Under the ``[test_name]`` section, specity the test parameters as a sequence of ``--browsertime.key=value`` arguments as a value of ``browsertime_args =``
  * Under the ``[test_name]`` section, override any other values as needed

* Add test as a subtest to run for Desktop ``taskcluster/ci/test/browsertime-desktop.yml`` (maybe also for mobile)
* Add test documentation to ``testing/raptor/raptor/perfdocs/config.yml``

* Generated files:

  * Run ``./mach lint --warnings --outgoing --fix`` to regenerate the documentation and task files, and warn about omissions
  * Running ``./mach lint -l perfdocs --fix .`` may also be needed

* Testing:

  * Test the new test by running it with the profiler
  * Test the new test by running it with a different browser
  * Test the new test by triggering it on CI
