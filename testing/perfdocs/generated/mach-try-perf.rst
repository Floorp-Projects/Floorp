#############
Mach Try Perf
#############

.. contents::
   :depth: 2
   :local:

To make it easier for developers to find the tests they need to run we built a perf-specific try selector called ``./mach try perf``. With this tool, you no longer need to remember the obfuscated platform and test names that you need to target for your tests. Instead, the new interface shows test categories along with a simplified name of the platform that they will run on.

When you trigger a try run from the perf selector, two try runs will be created. One with your changes, and one without. In your console, after you trigger the try runs, you'll find a PerfCompare link that will bring you directly to a comparison of the two pushes when they have completed.

The tool is built to be conservative about the number of tests to run, so if you are looking for something that is not listed, it's likely hidden behind a flag found in the ``--help``. Here's a list of what you'll find there::

    $ ./mach try perf --help

    optional arguments:
      -h, --help            show this help message and exit
    perf arguments:
      --show-all            Show all available tasks.
      --android             Show android test categories (disabled by default).
      --chrome              Show tests available for Chrome-based browsers
                            (disabled by default).
      --custom-car          Show tests available for Custom Chromium-as-Release
                            (disabled by default).
      --safari              Show tests available for Safari (disabled by default).
      --live-sites          Run tasks with live sites (if possible). You can also
                            use the `live-sites` variant.
      --profile             Run tasks with profiling (if possible). You can also
                            use the `profiling` variant.
      --single-run          Run tasks without a comparison
      -q QUERY, --query QUERY
                            Query to run in either the perf-category selector, or
                            the fuzzy selector if --show-all is provided.
      --browsertime-upload-apk BROWSERTIME_UPLOAD_APK
                            Path to an APK to upload. Note that this will replace
                            the APK installed in all Android Performance tests. If
                            the Activity, Binary Path, or Intents required change
                            at all relative to the existing GeckoView, and Fenix
                            tasks, then you will need to make fixes in the
                            associated taskcluster files (e.g.
                            taskcluster/ci/test/browsertime-mobile.yml).
                            Alternatively, set MOZ_FIREFOX_ANDROID_APK_OUTPUT to a
                            path to an APK, and then run the command with
                            --browsertime-upload-apk firefox-android. This option
                            will only copy the APK for browsertime, see
                            --mozperftest-upload-apk to upload APKs for startup
                            tests.
      --mozperftest-upload-apk MOZPERFTEST_UPLOAD_APK
                            See --browsertime-upload-apk. This option does the
                            same thing except it's for mozperftest tests such as
                            the startup ones. Note that those tests only exist
                            through --show-all, as they aren't contained in any
                            existing categories.
      --detect-changes      Adds a task that detects performance changes using
                            MWU.
      --comparator COMPARATOR
                            Either a path to a file to setup a custom comparison,
                            or a builtin name. See the Firefox source docs for
                            mach try perf for examples of how to build your own,
                            along with the interface.
      --comparator-args [ARG=VALUE [ARG=VALUE ...]]
                            Arguments provided to the base, and new revision setup
                            stages of the comparator.
      --variants [ [ ...]]  Select variants to display in the selector from:
                            fission, bytecode-cached, live-sites, profiling, swr
      --platforms [ [ ...]]
                            Select specific platforms to target. Android only
                            available with --android. Available platforms:
                            android-a51, android, windows, linux, macosx, desktop
      --apps [ [ ...]]      Select specific applications to target from: firefox,
                            chrome, chromium, geckoview, fenix, chrome-m, safari,
                            custom-car
      --clear-cache         Deletes the try_perf_revision_cache file
      --extra-args [ [ ...]]
                            Set the extra args (e.x, --extra-args verbose post-
                            startup-delay=1)
    task configuration arguments:
      --artifact            Force artifact builds where possible.
      --no-artifact         Disable artifact builds even if being used locally.
      --browsertime         Use browsertime during Raptor tasks.
      --disable-pgo         Don't run PGO builds
      --env ENV             Set an environment variable, of the form FOO=BAR. Can
                            be passed in multiple times.
      --gecko-profile       Create and upload a gecko profile during talos/raptor
                            tasks.
      --gecko-profile-interval GECKO_PROFILE_INTERVAL
                            How frequently to take samples (ms)
      --gecko-profile-entries GECKO_PROFILE_ENTRIES
                            How many samples to take with the profiler
      --gecko-profile-features GECKO_PROFILE_FEATURES
                            Set the features enabled for the profiler.
      --gecko-profile-threads GECKO_PROFILE_THREADS
                            Comma-separated list of threads to sample.
      paths                 Run tasks containing tests under the specified
                            path(s).
      --rebuild [2-20]      Rebuild all selected tasks the specified number of
                            times.



Workflow
--------

Below, you'll find an overview of the features available in ``./mach try perf``. If you'd like to learn more about how to use this tool to enhance your developement process, see the :ref:`Standard Workflow with Mach Try Perf` page.

Standard Usage
--------------

To use mach try perf simply call ``./mach try perf``. This will open an interface for test selection like so:


.. image:: ./standard-try-perf.png
   :alt: Mach try perf with default options
   :scale: 75%
   :align: center


Select the categories you'd like to run, hit enter, and wait for the tool to finish the pushes. **Note that it can take some time to do both pushes, and you might not see logging for some time.**

.. _Running Alert Tests:

Running Alert Tests
-------------------

To run all the tests that triggered a given alert, use ``./mach try perf --alert <ALERT-NUMBER>``. **It's recommended to use this when working with performance alerts.** The alert number can be found in comment 0 on any alert bug `such as this one <https://bugzilla.mozilla.org/show_bug.cgi?id=1844510>`_. As seen in the image below, the alert number can be found just above the summary table.

.. image:: ./comment-zero-alert-number.png
   :alt: Comment 0 containing an alert number just above the table.
   :scale: 50%
   :align: center


Chrome and Android
------------------

Android and chrome tests are disabled by default as they are often unneeded and waste our limited resources. If you need either of these, you can add ``--chrome`` and/or ``--android`` to the command like so ``./mach try perf --android --chrome``:


.. image:: ./android-chrome-try-perf.png
   :alt: Mach try perf with android, and chrome options
   :scale: 75%
   :align: center


Variants
--------

If you are looking for any variants (e.g. no-fission, bytecode-cached, live-sites), use the ``--variants`` options like so ``./mach try perf --variants live-sites``. This will select all possible categories that could have live-sites tests.


.. image:: ./variants-try-perf.png
   :alt: Mach try perf with variants
   :scale: 75%
   :align: center


Note that it is expected that the offered categories have extra variants (such as bytecode-cached) as we are showing all possible combinations that can include live-sites.

Platforms
---------

To target a particular platform you can use ``--platforms`` to only show categories with the given platforms.

Categories
----------

In the future, this section will be populated dynamically. If you are wondering what the categories you selected will run, you can use ``--no-push`` to print out a list of tasks that will run like so::

   $ ./mach try perf --no-push

   Artifact builds enabled, pass --no-artifact to disable
   Gathering tasks for Benchmarks desktop category
   Executing queries: 'browsertime 'benchmark, !android 'shippable !-32 !clang, !live, !profil, !chrom
   estimates: Runs 66 tasks (54 selected, 12 dependencies)
   estimates: Total task duration 8:45:58
   estimates: In the top 62% of durations
   estimates: Should take about 1:04:58 (Finished around 2022-11-22 15:08)
   Commit message:
   Perf selections=Benchmarks desktop (queries='browsertime 'benchmark&!android 'shippable !-32 !clang&!live&!profil&!chrom)
   Pushed via `mach try perf`
   Calculated try_task_config.json:
   {
       "env": {
           "TRY_SELECTOR": "fuzzy"
       },
       "tasks": [
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-ares6",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-assorted-dom",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-jetstream2",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-matrix-react-bench",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-animometer",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-htmlsuite",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-speedometer",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-stylebench",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-sunspider",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-twitch-animation",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-unity-webgl",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-webaudio",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-baseline",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-optimizing",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-baseline",
           "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-optimizing",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-ares6",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-assorted-dom",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-jetstream2",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-matrix-react-bench",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-animometer",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-htmlsuite",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-speedometer",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-stylebench",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-sunspider",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-twitch-animation",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-unity-webgl",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-firefox-webaudio",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-baseline",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-optimizing",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-baseline",
           "test-macosx1015-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-optimizing",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-ares6",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-assorted-dom",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-jetstream2",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-matrix-react-bench",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-animometer",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-htmlsuite",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-speedometer",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-stylebench",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-sunspider",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-twitch-animation",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-unity-webgl",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-firefox-webaudio",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-baseline",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-optimizing",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-baseline",
           "test-windows10-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-optimizing"
       ],
       "use-artifact-builds": true,
       "version": 1
   }


Adding a New Category
---------------------

It's very easy to add a new category if needed, and you can do so by modifying the `PerfParser categories attribute here <https://searchfox.org/mozilla-central/source/tools/tryselect/selectors/perf.py#179>`_. The following is an example of a complex category that gives a good idea of what you have available::

     "Resource Usage": {
         "query": {
             "talos": ["'talos 'xperf | 'tp5"],
             "raptor": ["'power 'osx"],
             "awsy": ["'awsy"],
         },
         "suites": ["talos", "raptor", "awsy"],
         "platform-restrictions": ["desktop"],
         "variant-restrictions": {
             "raptor": [],
             "talos": [],
         },
         "app-restrictions": {
             "raptor": ["firefox"],
             "talos": ["firefox"],
         },
         "tasks": [],
     },

The following fields are available:
     * **query**: Set the queries to use for each suite you need.
     * **suites**: The suites that are needed for this category.
     * **tasks**: A hard-coded list of tasks to select.
     * **platform-restrictions**: The platforms that it can run on.
     * **app-restrictions**: A list of apps that the category can run.
     * **variant-restrictions**: A list of variants available for each suite.

Note that setting the App/Variant-Restriction fields should be used to restrict the available apps and variants, not expand them as the suites, apps, and platforms combined already provide the largest coverage. The restrictions should be used when you know certain things definitely won't work, or will never be implemented for this category of tests. For instance, our ``Resource Usage`` tests only work on Firefox even though they may exist in Raptor which can run tests with Chrome.

Comparators
-----------

If the standard/default push-to-try comparison is not enough, you can build your own "comparator" that can setup the base, and new revisions. The default comparator ``BasePerfComparator`` runs the standard mach-try-perf comparison, and there also exists a custom comparator called ``BenchmarkComparator`` for running custom benchmark comparisons on try (using Github PR links).

If you'd like to add a custom comparator, you can either create it in a separate file and pass it in the ``--comparator``, or add it to the ``tools/tryselect/selectors/perfselector/perfcomparators.py`` and use the name of the class as the ``--comparator`` argument (e.g. ``--comparator BenchmarkComparator``). You can pass additional arguments to it using the ``--comparator-args`` option that accepts arguments in the format ``NAME=VALUE``.

The custom comparator needs to be a subclass of ``BasePerfComparator``, and optionally overrides its methods. See the comparators file for more information about the interface available. Here's the general interface for it (subject to change), note that the ``@comparator`` decorator is required when making a builtin comparator::

    @comparator
    class BasePerfComparator:
        def __init__(self, vcs, compare_commit, current_revision_ref, comparator_args):
            """Initialize the standard/default settings for Comparators.

            :param vcs object: Used for updating the local repo.
            :param compare_commit str: The base revision found for the local repo.
            :param current_revision_ref str: The current revision of the local repo.
            :param comparator_args list: List of comparator args in the format NAME=VALUE.
            """

        def setup_base_revision(self, extra_args):
            """Setup the base try run/revision.

            The extra_args can be used to set additional
            arguments for Raptor (not available for other harnesses).

            :param extra_args list: A list of extra arguments to pass to the try tasks.
            """

        def teardown_base_revision(self):
            """Teardown the setup for the base revision."""

        def setup_new_revision(self, extra_args):
            """Setup the new try run/revision.

            Note that the extra_args are reset between the base, and new revision runs.

            :param extra_args list: A list of extra arguments to pass to the try tasks.
            """

        def teardown_new_revision(self):
            """Teardown the new run/revision setup."""

        def teardown(self):
            """Teardown for failures.

            This method can be used for ensuring that the repo is cleaned up
            when a failure is hit at any point in the process of doing the
            new/base revision setups, or the pushes to try.
            """

Frequently Asked Questions (FAQ)
--------------------------------

If you have any questions which aren't already answered below please reach out to us in the `perftest matrix channel <https://matrix.to/#/#perftest:mozilla.org>`_.

     * **How can I tell what a category or a set of selections will run?**

       At the moment, you need to run your command with an additional option to see what will be run: ``./mach try perf --no-push``. See the `Categories`_ section for more information about this. In the future, we plan on having an dynamically updated list for the tasks in the `Categories`_ section of this document.

     * **What's the difference between ``Pageload desktop``, and ``Pageload desktop firefox``?**

       If you simply ran ``./mach try perf`` with no additional options, then there is no difference. If you start adding additional browsers to the try run with commands like ``./mach try perf --chrome``, then ``Pageload desktop`` will select all tests available for ALL browsers available, and ``Pageload desktop firefox`` will only select Firefox tests. When ``--chrome`` is provided, you'll also see a ``Pageload desktop chrome`` option.

     * **Help! I can't find a test in any of the categories. What should I do?**

       Use the option ``--show-all``. This will let you select tests from the ``./mach try fuzzy --full`` interface directly instead of the categories. You will always be able to find your tests this way. Please be careful with your task selections though as it's easy to run far too many tests in this way!

Future Work
-----------

The future work for this tool can be `found in this bug <https://bugzilla.mozilla.org/show_bug.cgi?id=1799178>`_. Feel free to file improvments, and bugs against it.
