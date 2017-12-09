Empty Selector
==============

The ``mach try empty`` subcommand is very simple, it won't schedule any additional tasks. You'll
still see lint tasks and python-unittest tasks if applicable, this is due to a configuration option
in taskcluster.

Other than those, your try run will be empty. You can use treeherder's ``Add new jobs`` feature to
selectively add additional tasks after the fact.

.. note::

    To use ``Add new jobs`` you'll need to be logged in and have commit access level 1, just as if
    you were pushing to try.

To do this:

    1. Click the drop-down arrow at the top right of your commit.
    2. Select ``Add new jobs`` (it may take a couple seconds to load).
    3. Choose your desired tasks by clicking them one at a time.
    4. At the top of your commit, select ``Trigger New Jobs``.

