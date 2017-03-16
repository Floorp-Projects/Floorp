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

A ``kind.yml`` file contains data about the kind, as well as referring to a
Python class implementing the kind in its ``implementation`` key.  That
implementation may rely on lots of code shared with other kinds, or contain a
completely unique implementation of some functionality.

The full list of pre-defined keys in this file is:

``implementation``
   Class implementing this kind, in the form ``<module-path>:<object-path>``.
   This class should be a subclass of ``taskgraph.kind.base:Kind``.

``kind-dependencies``
   Kinds which should be loaded before this one.  This is useful when the kind
   will use the list of already-created tasks to determine which tasks to
   create, for example adding an upload-symbols task after every build task.

Any other keys are subject to interpretation by the kind implementation.

The result is a nice segmentation of implementation so that the more esoteric
in-tree projects can do their crazy stuff in an isolated kind without making
the bread-and-butter build and test configuration more complicated.

Dependencies
------------

Dependencies between tasks are represented as labeled edges in the task graph.
For example, a test task must depend on the build task creating the artifact it
tests, and this dependency edge is named 'build'.  The task graph generation
process later resolves these dependencies to specific taskIds.

Decision Task
-------------

The decision task is the first task created when a new graph begins.  It is
responsible for creating the rest of the task graph.

The decision task for pushes is defined in-tree, in ``.taskcluster.yml``.  That
task description invokes ``mach taskcluster decision`` with some metadata about
the push.  That mach command determines the optimized task graph, then calls
the TaskCluster API to create the tasks.

Note that this mach command is *not* designed to be invoked directly by humans.
Instead, use the mach commands described below, supplying ``parameters.yml``
from a recent decision task.  These commands allow testing everything the
decision task does except the command-line processing and the
``queue.createTask`` calls.

Graph Generation
----------------

Graph generation, as run via ``mach taskgraph decision``, proceeds as follows:

#. For all kinds, generate all tasks.  The result is the "full task set"
#. Create dependency links between tasks using kind-specific mechanisms.  The
   result is the "full task graph".
#. Filter the target tasks (based on a series of filters, such as try syntax,
   tree-specific specifications, etc). The result is the "target task set".
#. Based on the full task graph, calculate the transitive closure of the target
   task set.  That is, the target tasks and all requirements of those tasks.
   The result is the "target task graph".
#. Optimize the target task graph using task-specific optimization methods.
   The result is the "optimized task graph" with fewer nodes than the target
   task graph.  See :ref:`optimization`.
#. Morph the graph. Morphs are like syntactic sugar: they keep the same meaning,
   but express it in a lower-level way. These generally work around limitations
   in the TaskCluster platform, such as number of dependencies or routes in
   a task.
#. Create tasks for all tasks in the morphed task graph.

Transitive Closure
..................

Transitive closure is a fancy name for this sort of operation:

 * start with a set of tasks
 * add all tasks on which any of those tasks depend
 * repeat until nothing changes

The effect is this: imagine you start with a linux32 test job and a linux64 test job.
In the first round, each test task depends on the test docker image task, so add that image task.
Each test also depends on a build, so add the linux32 and linux64 build tasks.

Then repeat: the test docker image task is already present, as are the build
tasks, but those build tasks depend on the build docker image task.  So add
that build docker image task.  Repeat again: this time, none of the tasks in
the set depend on a task not in the set, so nothing changes and the process is
complete.

And as you can see, the graph we've built now includes everything we wanted
(the test jobs) plus everything required to do that (docker images, builds).


Action Tasks
------------

Action Tasks are tasks which help you to schedule new jobs via Treeherder's
"Add New Jobs" feature. The Decision Task creates a YAML file named
``action.yml`` which can be used to schedule Action Tasks after suitably replacing
``{{decision_task_id}}`` and ``{{task_labels}}``, which correspond to the decision
task ID of the push and a comma separated list of task labels which need to be
scheduled.

This task invokes ``mach taskgraph action-task`` which builds up a task graph of
the requested tasks. This graph is optimized using the tasks running initially in
the same push, due to the decision task.

So for instance, if you had already requested a build task in the ``try`` command,
and you wish to add a test which depends on this build, the original build task
is re-used.

Action Tasks are currently scheduled by
[pulse_actions](https://github.com/mozilla/pulse_actions). This feature is only
present on ``try`` pushes for now.

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
:doc:`parameters`; using that information, you may modify an existing
``parameters.yml`` or create your own.

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
    Multiple labels may be substituted in a single string, and ``<<>`` can be
    used to escape a literal ``<``.

Taskgraph JSON Format
---------------------

Task graphs -- both the graph artifacts produced by the decision task and those
output by the ``--json`` option to the ``mach taskgraph`` commands -- are JSON
objects, keyed by label, or for optimized task graphs, by taskId.  For
convenience, the decision task also writes out ``label-to-taskid.json``
containing a mapping from label to taskId.  Each task in the graph is
represented as a JSON object.

Each task has the following properties:

``kind``
   The name of this task's kind

``task_id``
   The task's taskId (only for optimized task graphs)

``label``
   The task's label

``attributes``
   The task's attributes

``dependencies``
   The task's in-graph dependencies, represented as an object mapping
   dependency name to label (or to taskId for optimized task graphs)

``optimizations``
   The optimizations to be applied to this task

``task``
   The task's TaskCluster task definition.

The results from each command are in the same format, but with some differences
in the content:

* The ``tasks`` and ``target`` subcommands both return graphs with no edges.
  That is, just collections of tasks without any dependencies indicated.

* The ``optimized`` subcommand returns tasks that have been assigned taskIds.
  The dependencies array, too, contains taskIds instead of labels, with
  dependencies on optimized tasks omitted.  However, the ``task.dependencies``
  array is populated with the full list of dependency taskIds.  All task
  references are resolved in the optimized graph.

The output of the ``mach taskgraph`` commands are suitable for processing with
the `jq <https://stedolan.github.io/jq/>`_ utility.  For example, to extract all
tasks' labels and their dependencies:

.. code-block:: shell

    jq 'to_entries | map({label: .value.label, dependencies: .value.dependencies})'

