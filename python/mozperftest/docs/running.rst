Running a performance test
==========================

You can run `perftest` locally or in Mozilla's CI

Running locally
---------------

Running a test is as simple as calling it using `mach perftest` in a mozilla-central source
checkout::

    $ ./mach perftest perftest_script.js

The mach command will bootstrap the installation of all required tools for the framework to run.
You can use `--help` to find out about all options.


Running in the CI
-----------------

You can run in the CI directly from the `mach perftest` command by adding the `--push-to-try` option
to your locally working perftest call. We have phones on bitbar that can run your android tests.
Tests are fairly fast to run in the CI because they use sparse profiles. Depending on the
availability of workers, once the task starts, it takes around 15mn to start the test.

.. warning::

   If you plan to run tests often in the CI for android, you should contact the android
   infra team to make sure there's availability in our pool of devices.

