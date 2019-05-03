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
</taskcluster/taskcluster/try>`.

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

Specifying Queries on the Command Line
--------------------------------------

Sometimes it's more convenient to skip the interactive interface and specify a query on the command
line with ``-q/--query``. This is equivalent to opening the interface then typing:
``<query><ctrl-a><enter>``.

For example:

.. code-block:: shell

    # selects all mochitest tasks
    $ mach try fuzzy --query "mochitest"

You can pass in multiple queries at once and the results of each will be joined together:

.. code-block:: shell

    # selects all mochitest and reftest tasks
    $ mach try fuzzy -q "mochitest" -q "reftest"

If instead you want the intersection of queries, you can pass in ``-x/--and``:

.. code-block:: shell

    # selects all windows mochitest tasks
    $ mach try fuzzy --and -q "mochitest" -q "windows"


Modifying Presets
~~~~~~~~~~~~~~~~~

:doc:`Presets <../presets>` make it easy to run a pre-determined set of tasks. But sometimes you
might not want to run that set exactly as is, you may only want to use the preset as a starting
point then add or remove tasks as needed. This can be accomplished with ``-q/--query`` or
``-i/--interactive``.

Here are some examples of adding tasks to a preset:

.. code-block:: shell

    # selects all perf tasks plus all mochitest-chrome tasks
    $ mach try fuzzy --preset perf -q "mochitest-chrome"

    # adds tasks to the perf preset interactively
    $ mach try fuzzy --preset perf -i

Similarly, ``-x/--and`` can be used to filter down a preset by taking the intersection of the two
sets:

.. code-block:: shell

    # limits perf tasks to windows only
    $ mach try fuzzy --preset perf -xq "windows"

    # limits perf tasks interactively
    $ mach try fuzzy --preset perf -xi


Shell Conflicts
~~~~~~~~~~~~~~~

Unfortunately ``fzf``'s query language uses some characters (namely ``'``, ``!`` and ``$``) that can
interfere with your shell when using ``-q/--query``. Below are some tips for how to type out a query
on the command line.

The ``!`` character is typically used for history expansion. If you don't use this feature, the
easiest way to specify queries on the command line is to disable it:

.. code-block:: shell

    # bash
    $ set +H
    $ ./mach try fuzzy -q "'foo !bar"

    # zsh
    $ setopt no_banghist
    $ ./mach try fuzzy -q "'foo !bar"

If using ``bash``, add ``set +H`` to your ``~/.bashrc``, ``~/.bash_profile`` or equivalent. If using
``zsh``, add ``setopt no_banghist`` to your ``~/.zshrc`` or equivalent.

If you don't want to disable history expansion, you can escape your queries like this:

.. code-block:: shell

    # bash
    $ ./mach try fuzzy -q $'\'foo !bar'

    # zsh
    $ ./mach try fuzzy -q "'foo \!bar"


The third option is to use ``-e/--exact`` which reverses the behaviour of the ``'`` character (see
:ref:`additional-arguments` for more details). Using this flag means you won't need to escape the
``'`` character as often and allows you to run your queries like this:

.. code-block:: shell

    # bash and zsh
    $ ./mach try fuzzy -eq 'foo !bar'

This method is only useful if you find you almost always prefix terms with ``'`` (and rarely use
fuzzy terms). Otherwise as soon as you want to use a fuzzy match you'll run into the same problem as
before.

.. note:: All the examples in these three approaches will select the same set of tasks.

If you use ``fish`` shell, you won't need to escape ``!``, however you will need to escape ``$``:

.. code-block:: shell

    # fish
    $ ./mach try fuzzy -q "'foo !bar baz\$"


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

Paths can be used with the interactive ``fzf`` window, or using the ``-q/--query`` argument.
For example, running:

.. code-block:: shell

    $ mach try fuzzy layout/reftests/reftest-sanity -q "!pgo !cov !asan 'linux64"

Would produce the following ``try_task_config.json``:

.. code-block:: json

    {
      "templates":{
        "env":{
          "MOZHARNESS_TEST_PATHS":"{\"reftest\":\"layout/reftests/reftest-sanity\"}"
        }
      },
      "tasks":[
        "test-linux64-qr/debug-reftest-e10s-1",
        "test-linux64-qr/opt-reftest-e10s-1",
        "test-linux64/debug-reftest-e10s-1",
        "test-linux64/debug-reftest-no-accel-e10s-1",
        "test-linux64/opt-reftest-e10s-1",
        "test-linux64/opt-reftest-no-accel-e10s-1",
      ]
    }

Inside of these tasks, the reftest harness will only run tests that live under
``layout/reftests/reftest-sanity``.


.. _additional-arguments:

Additional Arguments
--------------------

There are a few additional command line arguments you may wish to use:

``-e/--exact``
By default, ``fzf`` treats terms as a fuzzy match and prefixing a term with ``'`` turns it into an exact
match. If passing in ``--exact``, this behaviour is reversed. Non-prefixed terms become exact, and a
``'`` prefix makes a term fuzzy.

``--full``
By default, only target tasks (e.g tasks that would normally run on mozilla-central)
are generated. Passing in ``--full`` allows you to select from all tasks. This is useful for
things like nightly or release tasks.

``-u/--update``
Update the bootstrapped ``fzf`` binary to the latest version.

For a full list of command line arguments, run:

.. code-block:: shell

    $ mach try fuzzy --help

For more information on using ``fzf``, run:

.. code-block:: shell

    $ man fzf

.. _fzf: https://github.com/junegunn/fzf
