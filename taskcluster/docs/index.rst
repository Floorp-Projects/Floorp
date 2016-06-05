.. taskcluster_index:

TaskCluster Task-Graph Generation
=================================

The ``taskcluster`` directory contains support for defining the graph of tasks
that must be executed to build and test the Gecko tree.  This is more complex
than you might suppose!  This implementation supports:

 * A huge array of tasks
 * Different behavior for different repositories
 * "Try" pushes, with special means to select a subset of the graph for execution
 * Optimization -- skipping tasks that have already been performed

.. toctree::

    taskgraph
    parameters
    attributes
    yaml-templates
    old
