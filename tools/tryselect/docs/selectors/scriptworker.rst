Scriptworker Selector
=====================

This command runs a selection of scriptworker tasks against builds from a
recent release.  This is aimed at release engineering, to test changes to
scriptworker implementations. It currently requires being connected to
Mozilla's internal datacenter VPN with access to shipit\ [#shipit]_.

There are a number of preset groups of tasks to run. To run a particular
set of tasks, pass the name of the set to ``mach try scriptworker``:

.. code-block:: shell

   $ mach try scriptworker linux-signing

To get the list of task sets, along with the list of tasks they will run:

.. code-block:: shell

   $ mach try scriptworker list

The selector defaults to using tasks from the most recent beta, to use tasks
from a different release, pass ``--release-type <release-type>``:

.. code-block:: shell

   $ mach try scriptworker --release-type release linux-signing


.. [#shipit] The shipit API is not currently publicly available, and is used
   to find the release graph to pull previous tasks from.
