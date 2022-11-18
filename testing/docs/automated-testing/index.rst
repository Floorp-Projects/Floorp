Automated Testing
=================

You've just written a feature and (hopefully!) want to test it. Or you've
decided that an existing feature doesn't have enough tests and want to contribute
some. But where do you start? You've looked around and found references to things
like "xpcshell" or "web-platform-tests" or "talos". What code, features or
platforms do they all test? Where do their feature sets overlap? In short, where
should your new tests go? This document is a starting point for those who want
to start to learn about Mozilla's automated testing tools and procedures. Below
you'll find a short summary of each framework we use, and some questions to help
you pick the framework(s) you need for your purposes.

If you still have questions, ask on `Matrix <https://wiki.mozilla.org/Matrix>`__
or on the relevant bug.

Firefox Production
------------------
These tests are found within the `mozilla-central <https://hg.mozilla.org/mozilla-central>`__
tree, along with the product code.

They are run when a changeset is pushed
to `mozilla-central <https://hg.mozilla.org/mozilla-central>`__,
`autoland <https://hg.mozilla.org/integration/autoland/>`__, or
`try </tools/try/index.html>`_, with the results showing up on
`Treeherder <https://treeherder.mozilla.org/>`__. Not all tests will be run on
every changeset; alogrithms are put in place to run the most likely failures,
with all tests being run on a regular basis.

They can also be run on local builds.

Linting
^^^^^^^

Lint tests help to ensure better quality, less error-prone code by analysing the
code with a linter.

.. list-table:: Linter Builders
    :header-rows: 1

    * - Name
      - Treeherder Symbol
      - What is Tested
      - Platforms
    * - `ESLint </code-quality/lint/linters/eslint.html>`__
      - ES
      - Javascript analysed for style and correctness.
      - All
    * - `eslint-build </code-quality/lint/linters/eslint.html#eslint-build-es-b>`__
      - ES-build
      - Extended javascript analysis that uses build artifacts.
      - All
    * - `flake8 </code-quality/lint/linters/flake8.html>`__
      - f8
      - Python analysed for style and correctness.
      - All

Functional testing
^^^^^^^^^^^^^^^^^^

.. list-table:: Functional Testing Builders
    :header-rows: 1

    * - Name
      - Treeherder Symbol
      - What is Tested
      - Platforms
    * - `xpcshell </testing/xpcshell/index.html>`__
      - X
      - Low-level code exposed to JavaScript, such as XPCOM components.
      - All
    * - `Mochitest browser-chrome </testing/mochitest-plain/index.html>`__
      - M(bc)
      - How the browser UI interacts with itself and with content.
      - All
    * - `Mochitest chrome </testing/mochitest-plain/index.html>`__
      - M(oth)
      - How the browser UI interacts with itself and with content.
      - All
    * - `Mochitest devtools </testing/mochitest-plain/index.html>`__
      - M(dt)
      - How the browser UI interacts with itself and with content.
      - All
    * - `Mochitest plain </testing/mochitest-plain/index.html>`__
      - M(1), M(2), ...
      - How the browser UI interacts with itself and with content.
      - All
