Selectors
=========

These are the currently implemented try selectors:

* :doc:`auto <auto>`: Have tasks chosen for you automatically.
* :doc:`fuzzy <fuzzy>`: Select tasks using a fuzzy finding algorithm and
  a terminal interface.
* :doc:`chooser <chooser>`: Select tasks using a web interface.
* :doc:`again <again>`: Re-run a previous ``try_task_config.json`` based
  push.
* :doc:`empty <empty>`: Don't select any tasks. Taskcluster will still run
  some tasks automatically (like lint and python unittest tasks). Further tasks
  can be chosen with treeherder's ``Add New Jobs`` feature.
* :doc:`syntax <syntax>`: Select tasks using classic try syntax.
* :doc:`release <release>`: Prepare a tree for doing a staging release.
* :doc:`scriptworker <scriptworker>`: Run scriptworker tasks against a recent release.
* :doc:`compare <compare>`: Push two identical try jobs, one on your current commit and another of your choice

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

  Auto <auto>
  Fuzzy <fuzzy>
  Chooser <chooser>
  Again <again>
  Empty <empty>
  Syntax <syntax>
  Release <release>
  Scriptworker <scriptworker>
