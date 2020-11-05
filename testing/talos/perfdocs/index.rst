=====
Talos
=====

Talos is a cross-platform Python performance testing framework that is specifically for
Firefox on desktop. New performance tests should be added to the newer framework
`mozperftest </testing/perfdocs/mozperftest.html>`_ unless there are limitations
there (highly unlikely) that make it absolutely necessary to add them to Talos. Talos is
named after the `bronze automaton from Greek myth <https://en.wikipedia.org/wiki/Talos>`_.

Talos tests are run in a similar manner to xpcshell and mochitests. They are started via
the command :code:`mach talos-test`. A `python script <https://searchfox.org/mozilla-central/source/testing/talos>`_
then launches Firefox, which runs the tests via JavaScript special powers. The test timing
information is recorded in a text log file, e.g. :code:`browser_output.txt`, and then processed
into the `JSON format supported by Perfherder <https://searchfox.org/mozilla-central/source/testing/mozharness/external_tools/performance-artifact-schema.json>`_.

Talos bugs can be filed in `Testing::Talos <https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Talos>`_.

Talos infrastructure is still mostly documented `on the Mozilla Wiki <https://wiki.mozilla.org/TestEngineering/Performance/Talos>`_.
In addition, there are plans to surface all of the individual tests using PerfDocs.
This work is tracked in `Bug 1674220 <https://bugzilla.mozilla.org/show_bug.cgi?id=1674220>`_.

Examples of current Talos runs can be `found in Treeherder by searching for "Talos" <https://treeherder.mozilla.org/jobs?repo=autoland&searchStr=Talos>`_.
If none are immediately available, then scroll to the bottom of the page and load more test
runs. The tests all share a group symbol starting with a :code:`T`, for example
:code:`T(c d damp g1)` or :code:`T-gli(webgl)`.

Running Talos Locally
*********************

Running tests locally is most likely only useful for debugging what is going on in a test,
as the test output is only reported as raw JSON. The CLI is documented via:

.. code-block:: bash

    ./mach talos-test --help

To quickly try out the :code:`./mach talos-test` command, the following can be run to do a
single run of the DevTools' simple netmonitor test.

.. code-block:: bash

    # Run the "simple.netmonitor" test very quickly with 1 cycle, and 1 page cycle.
    ./mach talos-test --activeTests damp --subtests simple.netmonitor --cycles 1 --tppagecycles 1


The :code:`--print-suites` and :code:`--print-tests` are two helpful command flags to
figure out what suites and tests are available to run.

.. code-block:: bash

    # Print out the suites:
    ./mach talos-test --print-suites

    # Available suites:
    #  bcv                          (basic_compositor_video)
    #  chromez                      (about_preferences_basic:tresize:about_newtab_with_snippets)
    #  dromaeojs                    (dromaeo_css:kraken)
    #  flex                         (tart_flex:ts_paint_flex)
    # ...

    # Run all of the tests in the "bcv" test suite:
    ./mach talos-test --suite bcv

    # Print out the tests:
    ./mach talos-test --print-tests

    # Available tests:
    # ================
    #
    # a11yr
    # -----
    # This test ensures basic a11y tables and permutations do not cause
    # performance regressions.
    #
    # about_newtab_with_snippets
    # --------------------------
    # Load about ActivityStream (about:home and about:newtab) with snippets enabled
    #
    # ...

    # Run the tests in "a11yr" listed above
    ./mach talos-test --activeTests a11yr

Running Talos on Try
********************

Talos runs can be generated through the mach try fuzzy finder:

.. code-block:: bash

    ./mach try fuzzy

The following is an example output at the time of this writing. Refine the query for the
platform and test suites of your choosing.

.. code-block::

    | test-windows10-64-qr/opt-talos-bcv-swr-e10s
    | test-linux64-shippable/opt-talos-webgl-e10s
    | test-linux64-shippable/opt-talos-other-e10s
    | test-linux64-shippable-qr/opt-talos-g5-e10s
    | test-linux64-shippable-qr/opt-talos-g4-e10s
    | test-linux64-shippable-qr/opt-talos-g3-e10s
    | test-linux64-shippable-qr/opt-talos-g1-e10s
    | test-windows10-64/opt-talos-webgl-gli-e10s
    | test-linux64-shippable/opt-talos-tp5o-e10s
    | test-linux64-shippable/opt-talos-svgr-e10s
    | test-linux64-shippable/opt-talos-flex-e10s
    | test-linux64-shippable/opt-talos-damp-e10s
    > test-windows7-32/opt-talos-webgl-gli-e10s
    | test-linux64-shippable/opt-talos-bcv-e10s
    | test-linux64-shippable/opt-talos-g5-e10s
    | test-linux64-shippable/opt-talos-g4-e10s
    | test-linux64-shippable/opt-talos-g3-e10s
    | test-linux64-shippable/opt-talos-g1-e10s
    | test-linux64-qr/opt-talos-bcv-swr-e10s

      For more shortcuts, see mach help try fuzzy and man fzf
      select: <tab>, accept: <enter>, cancel: <ctrl-c>, select-all: <ctrl-a>, cursor-up: <up>, cursor-down: <down>
      1379/2967
    > talos
