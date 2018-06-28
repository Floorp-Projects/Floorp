========
Overview
========

The task graph is built by linking different kinds of tasks together, pruning
out tasks that are not required, then optimizing by replacing subgraphs with
links to already-completed tasks.

Concepts
--------

* *Task Kind* - Tasks are grouped by kind, where tasks of the same kind
  have substantial similarities or share common processing logic. Kinds
  are the primary means of supporting diversity, in that a developer can
  add a new kind to do just about anything without impacting other kinds.

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

The result is a segmentation of implementation so that the more esoteric
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
   task graph.  See :doc:`optimization`.
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

This task invokes ``mach taskgraph action-callback`` which builds up a task graph of
the requested tasks. This graph is optimized using the tasks running initially in
the same push, due to the decision task.

So for instance, if you had already requested a build task in the ``try`` command,
and you wish to add a test which depends on this build, the original build task
is re-used.


Runnable jobs
-------------
As part of the execution of the Gecko decision task we generate a
``public/runnable-jobs.json.gz`` file. It contains a subset of all the data
contained within the ``full-task-graph.json``.

This file has the minimum amount of data needed by Treeherder to show all
tasks that can be scheduled on a push.


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

.. _taskgraph-graph-config:

Graph Configuration
-------------------

There are several configuration settings that are pertain to the entire
taskgraph. These are specified in :file:`config.yml` at the root of the
taskgraph configuration (typically :file:`taskcluster/ci/`). The available
settings are documented inline in `taskcluster/taskgraph/config.py
<https://dxr.mozilla.org/mozilla-central/source/taskcluster/taskgraph/config.py>`_.

.. _taskgraph-trust-domain:

Trust Domain
------------

When publishing and signing releases, that tasks verify their definition and
all upstream tasks come from a decision task based on a trusted tree. (see
`chain-of-trust verification <https://scriptworker.readthedocs.io/en/latest/chain_of_trust.html>`_).
Firefox and Thunderbird share the taskgraph code and in particular, they have
separate taskgraph configurations and in particular distinct decision tasks.
Although they use identical docker images and toolchains, in order to track the
province of those artifacts when verifying the chain of trust, they use
different index paths to cache those artifacts. The ``trust-domain`` graph
configuration controls the base path for indexing these cached artifacts.
