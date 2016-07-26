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
 * Extremely flexible generation of a variety of tasks using an approach of
   incrementally transforming job descriptions into task definitions.

This section of the documentation describes the process in some detail,
referring to the source where necessary.  If you are reading this with a
particular goal in mind and would rather avoid becoming a task-graph expert,
check out the :doc:`how-to section <how-tos>`.

.. toctree::

    taskgraph
    parameters
    attributes
    kinds
    transforms
    yaml-templates
    how-tos
    docker-images
