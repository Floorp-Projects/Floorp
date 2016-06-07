======================
TaskGraph Mach Command
======================

The task graph is built by linking different kinds of tasks together, pruning
out tasks that are not required, then optimizing by replacing subgraphs with
links to already-completed tasks.

Concepts
--------

* *Task Kind* - Tasks are grouped by kind, where tasks of the same kind do not
  have interdependencies but have substantial similarities, and may depend on
  tasks of other kinds.  Kinds are the primary means of supporting diversity,
  in that a developer can add a new kind to do just about anything without
  impacting other kinds.

* *Task Attributes* - Tasks have string attributes by which can be used for
  filtering.  Attributes are documented in :doc:`attributes`.

* *Task Labels* - Each task has a unique identifier within the graph that is
  stable across runs of the graph generation algorithm.  Labels are replaced
  with TaskCluster TaskIds at the latest time possible, facilitating analysis
  of graphs without distracting noise from randomly-generated taskIds.

* *Optimization* - replacement of a task in a graph with an equivalent,
  already-completed task, or a null task, avoiding repetition of work.

Kinds
-----

Kinds are the focal point of this system.  They provide an interface between
the large-scale graph-generation process and the small-scale task-definition
needs of different kinds of tasks.  Each kind may implement task generation
differently.  Some kinds may generate task definitions entirely internally (for
example, symbol-upload tasks are all alike, and very simple), while other kinds
may do little more than parse a directory of YAML files.

A `kind.yml` file contains data about the kind, as well as referring to a
Python class implementing the kind in its ``implementation`` key.  That
implementation may rely on lots of code shared with other kinds, or contain a
completely unique implementation of some functionality.

The result is a nice segmentation of implementation so that the more esoteric
in-tree projects can do their crazy stuff in an isolated kind without making
the bread-and-butter build and test configuration more complicated.

Dependencies
------------

Dependency links between tasks are always between different kinds(*).  At a
large scale, you can think of the dependency graph as one between kinds, rather
than between tasks.  For example, the unittest kind depends on the build kind.
The details of *which* tasks of the two kinds are linked is left to the kind
definition.

(*) A kind can depend on itself, though.  You can safely ignore that detail.
Tasks can also be linked within a kind using explicit dependencies.

Decision Task
-------------

The decision task is the first task created when a new graph begins.  It is
responsible for creating the rest of the task graph.

The decision task for pushes is defined in-tree, currently at
``testing/taskcluster/tasks/decision``.  The task description invokes ``mach
taskcluster decision`` with some metadata about the push.  That mach command
determines the optimized task graph, then calls the TaskCluster API to create
the tasks.

Graph Generation
----------------

Graph generation, as run via ``mach taskgraph decision``, proceeds as follows:

#. For all kinds, generate all tasks.  The result is the "full task set"
#. Create links between tasks using kind-specific mechanisms.  The result is
   the "full task graph".
#. Select the target tasks (based on try syntax or a tree-specific
   specification).  The result is the "target task set".
#. Based on the full task graph, calculate the transitive closure of the target
   task set.  That is, the target tasks and all requirements of those tasks.
   The result is the "target task graph".
#. Optimize the target task graph based on kind-specific optimization methods.
   The result is the "optimized task graph" with fewer nodes than the target
   task graph.
#. Create tasks for all tasks in the optimized task graph.

Optimization
------------

The objective of optimization to remove as many tasks from the graph as
possible, as efficiently as possible, thereby delivering useful results as
quickly as possible.  For example, ideally if only a test script is modified in
a push, then the resulting graph contains only the corresponding test suite
task.

A task is said to be "optimized" when it is either replaced with an equivalent,
already-existing task, or dropped from the graph entirely.

A task can be optimized if all of its dependencies can be optimized and none of
its inputs have changed.  For a task on which no other tasks depend (a "leaf
task"), the optimizer can determine what has changed by looking at the
version-control history of the push: if the relevant files are not modified in
the push, then it considers the inputs unchanged.  For tasks on which other
tasks depend ("non-leaf tasks"), the optimizer must replace the task with
another, equivalent task, so it generates a hash of all of the inputs and uses
that to search for a matching, existing task.

In some cases, such as try pushes, tasks in the target task set have been
explicitly requested and are thus excluded from optimization. In other cases,
the target task set is almost the entire task graph, so targetted tasks are
considered for optimization.  This behavior is controlled with the
``optimize_target_tasks`` parameter.

Mach commands
-------------

A number of mach subcommands are available aside from ``mach taskgraph
decision`` to make this complex system more accesssible to those trying to
understand or modify it.  They allow you to run portions of the
graph-generation process and output the results.

``mach taskgraph tasks``
   Get the full task set

``mach taskgraph full``
   Get the full task graph

``mach taskgraph target``
   Get the target task set

``mach taskgraph target-graph``
   Get the target task graph

``mach taskgraph optimized``
   Get the optimized task graph

Each of these commands taskes a ``--parameters`` option giving a file with
parameters to guide the graph generation.  The decision task helpfully produces
such a file on every run, and that is generally the easiest way to get a
parameter file.  The parameter keys and values are described in
:doc:`parameters`.

Finally, the ``mach taskgraph decision`` subcommand performs the entire
task-graph generation process, then creates the tasks.  This command should
only be used within a decision task, as it assumes it is running in that
context.

Taskgraph JSON Format
---------------------

Each task graph artifact is represented as a JSON object.  The object's
properties are the task labels or taskIds (see below), and the value of each
property describes a task in an object with the following attributes:

``label``
   The task's label (never a taskId).

``attributes``
   The task's attributes

``dependencies``
   The task's in-graph dependencies, each represented as a pair ``[name, label]``
   giving the dependency name and the label for the required task.

``task``
   The task's TaskCluster task definition.

The task definition may contain "task references" of the form
``{"task-reference": "string containing <task-label>"}``.  These will be
replaced during the optimization step, with the appropriate taskId substituted
for ``<task-label>`` in the string.  Multiple labels may be substituted in a
single string, and ``<<>`` can be used to escape a literal ``<``.

The results from each command are in the same format, but with some differences
in the content:

* The ``tasks`` and ``target`` subcommands both return graphs with no edges.
  That is, just collections of tasks without any dependencies indicated.

* The ``optimized`` subcommand returns a graph keyed by taskId rather than
  label.  The dependencies array, too, contains taskIds instead of labels.
  Dependencies on optimized tasks are omitted.  However, the
  ``task.dependencies`` array is populated with the full list of dependency
  taskIds.  All task references are resolved in the optimized graph.
