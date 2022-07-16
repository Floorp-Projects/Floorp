Taskgraph Overview
==================

Taskgraph builds a directed acyclic graph, where each node in the graph
represents a task, and each edge represents the dependency relationships
between them.

See Taskgraph's `graph generation documentation`_ for more details.

Decision Task
-------------

The decision task is the first task created when a new graph begins.  It is
responsible for creating the rest of the task graph.

The decision task for pushes is defined in-tree, in ``.taskcluster.yml``.  That
task description invokes ``mach taskcluster decision`` with some metadata about
the push. That mach command determines the required graph of tasks to submit,
then calls the Taskcluster API to create the tasks.

.. note::

   ``mach taskgraph decision`` is *not*  meant to be invoked by humans.
   Instead, follow `this guide`_ (prepending ``mach`` to all commands) to
   invoke Taskgraph locally without submitting anything to Taskcluster.

.. _kinds:

Kinds
-----

Generation starts with "kinds". These are yaml files that denote groupings of
tasks that are loosely related to one another. For example, in Gecko there are
``build`` and ``test`` kinds. Each kind has its own directory under
`taskcluster/ci`_ which contains a ``kind.yml`` file.

For more information on kinds, see Taskgraph's `kind documentation`_. For a
list of available kinds in ``mozilla-central``, see the :doc:`kinds reference
</taskcluster/kinds>`.

Loader
------

Next, a "loader" is responsible for parsing each ``kind.yml`` file and turning
it into an initial set of tasks. Loaders will always parse kinds in an ordering
satisfying the ``kind-dependencies`` key.

See Taskgraph's `loader documentation`_ for more details.

.. _transforms:

Transforms
----------

After the initial set of tasks are loaded, transformations are applied to each
task. Transforms are Python functions that take a generator of tasks as input,
and yields a generator of tasks as output. Possibly modifying, adding or removing
tasks along the way.

See Taskgrpah's `transforms documentation`_ for more details on transforms, or
the :doc:`transforms section </taskcluster/transforms/index>` for information
on some of the transforms available in ``mozilla-central``.

Target Tasks
------------

After transformation is finished, the `target_tasks`_ module filters out any
tasks that aren't applicable to the current :doc:`parameter set
</taskcluster/parameters>`.

Optimization
------------

After the "target graph" is generated, an optimization process looks to remove
or replace unnecessary tasks in the graph. For instance, a task may be removed
if the push doesn't modify any of the files that could affect it.

See Taskgraph's `optimization documentation`_ for more details, or the
:doc:`optimization strategies <optimization/index>` available in
``mozilla-central``.


Task Parameterization
---------------------

A few components of tasks are only known at the very end of the decision task
-- just before the ``queue.createTask`` call is made.  These are specified
using simple parameterized values, as follows:

``{"relative-datestamp": "certain number of seconds/hours/days/years"}``
    Objects of this form will be replaced with an offset from the current time
    just before the ``queue.createTask`` call is made.  For example, an
    artifact expiration might be specified as ``{"relative-datestamp": "1
    year"}``.

``{"task-reference": "string containing <dep-name>"}``
    The task definition may contain "task references" of this form.  These will
    be replaced during the optimization step, with the appropriate taskId for
    the named dependency substituted for ``<dep-name>`` in the string.
    Additionally, `decision` and `self` can be used a dependency names to refer
    to the decision task, and the task itself.  Multiple labels may be
    substituted in a single string, and ``<<>`` can be used to escape a literal
    ``<``.

``{"artifact-reference": "..<dep-name/artifact/name>.."}``
    Similar to a ``task-reference``, but this substitutes a URL to the queue's
    ``getLatestArtifact`` API method (for which a GET will redirect to the
    artifact itself).

.. _taskgraph-graph-config:

Graph Configuration
-------------------

There are several configuration settings that are pertain to the entire
taskgraph. These are specified in :file:`config.yml` at the root of the
taskgraph configuration (typically :file:`taskcluster/ci/`). The available
settings are documented inline in `taskcluster/gecko_taskgraph/config.py
<https://searchfox.org/mozilla-central/source/taskcluster/gecko_taskgraph/config.py>`_.

.. _taskgraph-trust-domain:

Action Tasks
------------

Action Tasks are tasks which perform an action based on a manual trigger (e.g
clicking a button in Treeherder). Actions are how it is possible to retrigger
or "Add New Jobs".

For more information, see Taskgraph's `actions documentation`_.

.. _graph generation documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/concepts/task-graphs.html
.. _this guide: https://taskcluster-taskgraph.readthedocs.io/en/latest/howto/run-locally.html
.. _taskcluster/ci: https://searchfox.org/mozilla-central/source/taskcluster/ci
.. _kind documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/concepts/kind.html
.. _loader documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/concepts/loading.html
.. _transforms documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/concepts/transforms.html
.. _target_tasks:
.. _optimization documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/concepts/optimization.html
.. _actions documentation: https://taskcluster-taskgraph.readthedocs.io/en/latest/howto/create-actions.html
