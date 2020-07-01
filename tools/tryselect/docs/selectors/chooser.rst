Chooser Selector
================

When pushing to try, there are a very large amount of builds and tests to choose from. Often too
many to remember, making it easy to forget a set of tasks which should otherwise have been run.

This selector allows you to select tasks from a web interface that lists all the possible build and
test tasks and allows you to select them from a list. It is similar in concept to the old `try
syntax chooser`_ page, except that the values are dynamically generated using the :ref:`taskgraph<TaskCluster Task-Graph Generation>` as an
input. This ensures that it will never be out of date.

To use:

.. code-block:: shell

    $ mach try chooser

This will spin up a local web server (using Flask) which serves the chooser app. After making your
selection, simply press ``Push`` and the rest will be handled from there. No need to copy/paste any
syntax strings or the like.

You can run:

.. code-block:: shell

    $ mach try chooser --full

To generate the interface using the full :ref:`taskgraph<TaskCluster Task-Graph Generation>` instead. This will include tasks that don't run
on mozilla-central.


.. _try syntax chooser: https://mozilla-releng.net/trychooser
