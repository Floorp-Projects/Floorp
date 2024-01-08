Performance scripts
===================

Performance scripts are programs that drive the browser to run a specific
benchmark (like a page load or a lower level call) and produce metrics.

We support two flavors right now in `perftest` (but it's easy to add
new ones):

- **xpcshell** a classical xpcshell test, turned into a performance test
- **browsertime** a browsertime script, which runs a full browser and controls
  it via a Selenium client.
- **mochitest** a classical mochitest test, turned into a performance test

In order to qualify as performance tests, both flavors require metadata.

For our supported flavors that are both Javascript modules, those are
provided in a `perfMetadata` mapping variable in the module, or in
the `module.exports` variable when using Node.

This is the list of fields:

- **owner**: name of the owner (person or team) [mandatory]
- **author**: author of the test
- **name**: name of the test [mandatory]
- **description**: short description [mandatory]
- **longDescription**: longer description
- **options**: options used to run the test
- **supportedBrowsers**: list of supported browsers (or "Any")
- **supportedPlatforms**: list of supported platforms (or "Any")
- **tags** a list of tags that describe the test

Tests are registered using tests manifests and the **PERFTESTS_MANIFESTS**
variable in `moz.build` files - it's good practice to name this file
`perftest.toml`.

Example of such a file: https://searchfox.org/mozilla-central/source/testing/performance/perftest.toml


xpcshell
--------

`xpcshell` tests are plain xpcshell tests, with two more things:

- the `perfMetadata` variable, as described in the previous section
- calls to `info("perfMetrics", ...)` to send metrics to the `perftest` framework.

Here's an example of such a metrics call::

    # compute some speed metrics
    let speed = 12345;
    info("perfMetrics", JSON.stringify({ speed }));


Mochitest
---------

Similar to ``xpcshell`` tests, these are standard ``mochitest`` tests with some extra things:

- the ``perfMetadata`` variable, as described in the previous section
- calls to ``info("perfMetrics", ...)`` to send metrics to the ``perftest`` framework

Note that the ``perfMetadata`` variable can exist in any ``<script>...</script>`` element in the Mochitest HTML test file. The ``perfMetadata`` variable also needs a couple additional settings in Mochitest tests. These are the ``manifest``, and ``manifest_flavor`` options::

    var perfMetadata = {
      owner: "Performance Team",
      name: "Test test",
      description: "N/A",
      options: {
        default: {
          perfherder: true,
          perfherder_metrics: [
            { name: "Registration", unit: "ms" },
          ],
          manifest: "perftest.toml",
          manifest_flavor: "plain",
          extra_args: [
            "headless",
          ]
        },
      },
    };

The ``extra_args`` setting provides an area to provide custom Mochitest command-line arguments for this test.

Here's an example of a call that will produce metrics::

    # compute some speed metrics
    let speed = 12345;
    info("perfMetrics", JSON.stringify({ speed }));

Existing Mochitest unit tests can be modified with these to be compatible with mozperftest, but note that some issues exist when doing this:

- unittest issues with mochitest tests running on hardware
- multiple configurations of a test running in a single manifest

At the top of this document, you can find some information about the recommended approach for adding a new manifest dedicated to running performance tests.

Locally, mozperftest uses ``./mach test`` to run your test. Always ensure that your test works in ``./mach test`` before attempting to run it through ``./mach perftest``. In CI, we use a custom "remote" run that runs Mochitest directly, skipping ``./mach test``.

If everything is setup correctly, running a performance test locally will be as simple as this::

    ./mach perftest <path/to/my/mochitest-test.html>


Browsertime
-----------

With the browsertime layer, performance scenarios are Node modules that
implement at least one async function that will be called by the framework once
the browser has started. The function gets a webdriver session and can interact
with the browser.

You can write complex, interactive scenarios to simulate a user journey,
and collect various metrics.

Full documentation is available `here <https://www.sitespeed.io/documentation/sitespeed.io/scripting/>`_

The mozilla-central repository has a few performance tests script in
`testing/performance` and more should be added in components in the future.

By convention, a performance test is prefixed with **perftest_** to be
recognized by the `perftest` command.

A performance test implements at least one async function published in node's
`module.exports` as `test`. The function receives two objects:

- **context**, which contains:

  - **options** - All the options sent from the CLI to Browsertime
  - **log** - an instance to the log system so you can log from your navigation script
  - **index** - the index of the runs, so you can keep track of which run you are currently on
  - **storageManager** - The Browsertime storage manager that can help you read/store files to disk
  - **selenium.webdriver** - The Selenium WebDriver public API object
  - **selenium.driver** - The instantiated version of the WebDriver driving the current version of the browser

- **command** provides API to interact with the browser. It's a wrapper
  around the selenium client `Full documentation here <https://www.sitespeed.io/documentation/sitespeed.io/scripting/#commands>`_


Below is an example of a test that visits the BBC homepage and clicks on a link.

.. sourcecode:: javascript

    "use strict";

    async function setUp(context) {
      context.log.info("setUp example!");
    }

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

    async function tearDown(context) {
      context.log.info("tearDown example!");
    }

    module.exports = {
        setUp,
        test,
        tearDown,
        owner: "Performance Team",
        test_name: "BBC",
        description: "Measures pageload performance when clicking on a link from the bbc.com",
        supportedBrowsers: "Any",
        supportePlatforms: "Any",
    };


Besides the `test` function, scripts can implement a `setUp` and a `tearDown` function to run
some code before and after the test. Those functions will be called just once, whereas
the `test` function might be called several times (through the `iterations` option)


Hooks
-----

A Python module can be used to run functions during a run lifecycle. Available hooks are:

- **before_iterations(args)** runs before everything is started. Gets the args, which
  can be changed. The **args** argument also contains a **virtualenv** variable that
  can be used for installing Python packages (e.g. through `install_package <https://searchfox.org/mozilla-central/source/python/mozperftest/mozperftest/utils.py#115-144>`_).
- **before_runs(env)** runs before the test is launched. Can be used to
  change the running environment.
- **after_runs(env)** runs after the test is done.
- **on_exception(env, layer, exception)** called on any exception. Provides the
  layer in which the exception occurred, and the exception. If the hook returns `True`
  the exception is ignored and the test resumes. If the hook returns `False`, the
  exception is ignored and the test ends immediately. The hook can also re-raise the
  exception or raise its own exception.

In the example below, the `before_runs` hook is setting the options on the fly,
so users don't have to provide them in the command line::

    from mozperftest.browser.browsertime import add_options

    url = "'https://www.example.com'"

    common_options = [("processStartTime", "true"),
                      ("firefox.disableBrowsertimeExtension", "true"),
                      ("firefox.android.intentArgument", "'-a'"),
                      ("firefox.android.intentArgument", "'android.intent.action.VIEW'"),
                      ("firefox.android.intentArgument", "'-d'"),
                      ("firefox.android.intentArgument", url)]


    def before_runs(env, **kw):
        add_options(env, common_options)


To use this hook module, it can be passed to the `--hooks` option::

    $  ./mach perftest --hooks hooks.py perftest_example.js


