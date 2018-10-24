Selectors
=========

These are the currently implemented try selectors:

* :doc:`fuzzy <fuzzy>`: Select tasks using a fuzzy finding algorithm and
  a terminal interface.
* :doc:`again <again>`: Re-run a previous ``try_task_config.json`` based
  push.
* :doc:`empty <empty>`: Don't select any tasks. Taskcluster will still run
  some tasks automatically (like lint and python unittest tasks). Further tasks
  can be chosen with treeherder's ``Add New Jobs`` feature.
* :doc:`syntax <syntax>`: Select tasks using classic try syntax.
* :doc:`release <release>`: Prepare a tree for doing a staging release.

You can run them with:

.. code-block:: shell

    $ mach try <selector>

See selector specific options by running:

.. code-block:: shell

    $ mach try <selector> --help

.. toctree::
  :caption: Available Selectors
  :maxdepth: 1
  :hidden:

  Fuzzy <fuzzy>
  Again <again>
  Empty <empty>
  Syntax <syntax>
  Release <release>
