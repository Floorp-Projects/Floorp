Fuzzy Selector
==============

The fuzzy selector uses a tool called `fzf`_. It allows you to filter down all of the task labels
from a terminal based UI and an intelligent fuzzy finding algorithm. If the ``fzf`` binary is not
installed, you'll be prompted to bootstrap it on first run.

Selecting Tasks
---------------

Running ``mach try fuzzy`` without arguments will open up the ``fzf`` interface with all of the
available tasks pre-populated on the left. If you start typing, you'll notice tasks are instantly
filtered with a fuzzy match. You can select tasks by ``Ctrl+Click`` with the mouse or by using the
following keyboard shortcuts:

.. code-block:: text

    Ctrl-K / Down  => Move cursor up
    Ctrl-J / Up    => Move cursor down
    Tab            => Select task + move cursor down
    Shift-Tab      => Select task + move cursor up
    Ctrl-A         => Select all currently filtered tasks
    Ctrl-D         => De-select all currently filtered tasks
    Ctrl-T         => Toggle select all currently filtered tasks
    Alt-Bspace     => Clear input
    ?              => Toggle preview pane

Notice you can type a query, select some tasks, clear the query and repeat. As you select tasks
notice they get listed on the right. This is the preview pane, it is a view of what will get
scheduled when you're done. When you are satisfied with your selection, press ``Enter`` and all the
tasks in the preview pane will be pushed to try. If you changed your mind you can press ``Esc`` or
``Ctrl-C`` to exit the interface without pushing anything to try.

Unlike the ``syntax`` selector, the ``fuzzy`` selector doesn't use the commit message to pass
information up to taskcluster. Instead, it uses a file that lives at the root of the repository
called ``try_task_config.json``. You can read more information in the :doc:`taskcluster docs
<taskcluster/try>`.

Extended Search
---------------

When typing in search terms, you can separate terms by spaces. These terms will be joined by the AND
operator. For example the query:

.. code-block:: text

    windows mochitest

Will match tasks that have both the terms ``windows`` AND ``mochitest`` in them. This is a fuzzy match so the query:

.. code-block:: text

    wndws mchtst

Would likely match a similar set of tasks. The following modifiers can be applied to a search term:

.. code-block:: text

    'word    => exact match (line must contain the literal string "word")
    ^word    => exact prefix match (line must start with literal "word")
    word$    => exact suffix match (line must end with literal "word")
    !word    => exact negation match (line must not contain literal "word")
    'a | 'b  => OR operator (joins two exact match operators together)

For example:

.. code-block:: text

    ^start 'exact | !ignore fuzzy end$

Test Paths
----------

One or more paths to a file or directory may be specified as positional arguments. When
specifying paths, the list of available tasks to choose from is filtered down such that
only suites that have tests in a specified path can be selected. Notably, only the first
chunk of each suite/platform appears. When the tasks are scheduled, only tests that live
under one of the specified paths will be run.

.. note::

    When using paths, be aware that all tests under the specified paths will run in the
    same chunk. This might produce a different ordering from what gets run on production
    branches, and may yield different results.

    For suites that restart the browser between each manifest (like mochitest), this
    shouldn't be as big of a concern.

Paths can be used with the interactive fzf window, or using the ``-q/--query`` argument.
For example, running:

.. code-block:: shell

    $ mach try fuzzy layout/reftests/reftest-sanity -q "!pgo !cov !asan 'linux64"

Would produce the following ``try_task_config.json``:

.. code-block:: json

    {
      "templates":{
        "env":{
          "MOZHARNESS_TEST_PATHS":"layout/reftests/reftest-sanity"
        }
      },
      "tasks":[
        "test-linux64-qr/debug-reftest-e10s-1",
        "test-linux64-qr/opt-reftest-e10s-1",
        "test-linux64-stylo-disabled/debug-reftest-e10s-1",
        "test-linux64-stylo-disabled/opt-reftest-e10s-1",
        "test-linux64/debug-reftest-e10s-1",
        "test-linux64/debug-reftest-no-accel-e10s-1",
        "test-linux64/debug-reftest-stylo-e10s-1",
        "test-linux64/opt-reftest-e10s-1",
        "test-linux64/opt-reftest-no-accel-e10s-1",
        "test-linux64/opt-reftest-stylo-e10s-1"
      ]
    }

Inside of these tasks, the reftest harness will only run tests that live under
``layout/reftests/reftest-sanity``.

Additional Arguments
--------------------

There are a few additional command line arguments you may wish to use:

``-q/--query``
Instead of opening the interactive interface, automatically apply the specified
query. This is equivalent to opening the interface then typing: ``<query><ctrl-a><enter>``.

``--full``
By default, only target tasks (e.g tasks that would normally run on mozilla-central)
are generated. Passing in ``--full`` allows you to select from all tasks. This is useful for
things like nightly or release tasks.

``-u/--update``
Update the bootstrapped fzf binary to the latest version.

For a full list of command line arguments, run:

.. code-block:: shell

    $ mach try fuzzy --help

For more information on using ``fzf``, run:

.. code-block:: shell

    $ man fzf

.. _fzf: https://github.com/junegunn/fzf
