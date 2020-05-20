===========
mozperftest
===========

**mozperftest** can be used to run performance tests against browsers. It
leverages the `browsertime.js <https://www.sitespeed.io/documentation/browsertime/>`_
framework and provides a full integration into Mozilla's build and CI systems.

Browsertime uses the selenium webdriver client to drive the browser,
and provides some metrics to measure performance during a user journey.

Performance scenarios are Javascript modules that implement an async
function that gets a webdriver session and can interact with the browser.

You can write complex, interactive scenarios to simulate a user journey,
and collect various metrics.

The mozilla-central repository has a few scripts in `testing/performance`
and more should be added in components in the future.

By convention, a performance test is prefixed with **perftest_** to be
recognized.


Writing a performance test
===========================

A performance test implements at least one async function published in node's
`module.exports` as `test`. You can also add some extra metatadata to describe
the test, like an owner, a description (XXX link to the full standard)

.. sourcecode:: javascript

    "use strict";

    async function test(context, commands) {
        await commands.navigate("https://www.bbc.com/");

        // Wait for browser to settle
        await commands.wait.byTime(10000);

        // Start the measurement
        await commands.measure.start("pageload");

        // Click on the link and wait for page complete check to finish.
        await commands.click.byClassNameAndWait("block-link__overlay-link");

        // Stop and collect the measurement
        await commands.measure.stop();
    }


    module.exports = {
        test,
        owner: "Performance Team",
        test_name: "BBC",
        description: "Measures pageload performance when clicking on a link from the bbc.com",
        long_description: ``
            This test launches the browser, navigates to https://www.bbc.com/ where it then settles.
            A news item link is selected by classname and clicked on. The resulting pageload metrics are captured.
        ``,
        supported_browser: "Any",
        platform: "Any",
    };


The **context** and **commands** objects, and everything you can do within a script, is
described `here <https://www.sitespeed.io/documentation/sitespeed.io/scripting/>`_.

The script capabilities are technically limitless since you can execute any JS code in the browser
via webdriver.


Running a performance test
==========================

Running a test is as simple as calling it using `mach perftest` in a mozilla-central source
checkout::

    $ ./mach perftest perftest_script.js

The mach command will bootstrap the installation of all required tools for the framework to run.
You can use `--help` to find out about all options.

XXX talk about hooks.


Running in the CI
=================

You can run in the CI directly from the `mach perftest` command by adding the `--push-to-try` option
to your locally working perftest call. We have phones on bitbar that can run your android tests.
Tests are fairly fast to run in the CI because they use sparse profiles. Depending on the
availability of workers, once the task starts, it takes around 15mn to start the test.

.. note::

   If you plan to run tests often in the CI for android, you should contact the android
   team to make sure there's availability in our pool of devices.


Developing in mozperftest
=========================

Before landing a patch for mozperftest, make sure you run::

    ./mach perftest-test

