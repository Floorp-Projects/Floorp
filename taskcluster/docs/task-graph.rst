Task Graph
==========

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

After a change to the Gecko source code is pushed to version-control,
jobs for that change appear
on `Treeherder <https://treeherder.mozilla.org/>`__. How does this
work?

-  A "decision task" is created to decide what to do with the push.
-  The decision task creates a lot of additional tasks. These tasks
   include build and test tasks, along with lots of other kinds of tasks
   to build docker images, build toolchains, perform analyses, check
   syntax, and so on.
-  These tasks are arranged in a "task graph", with some tasks (e.g.,
   tests) depending on others (builds). Once its prerequisite tasks
   complete, a dependent task begins.
-  The result of each task is sent to
   `TreeHerder <https://treeherder.mozilla.org>`__ where developers and
   sheriffs can track the status of the push.
-  The outputs from each task, log files, Firefox installers, and so on,
   appear attached to each task when it completes. These are viewable in
   the `Task
   Inspector <https://tools.taskcluster.net/task-inspector/>`__.

All of this is controlled from within the Gecko source code, through a
process called *task-graph generation*.  This means it's easy to add a
new job or tweak the parameters of a job in a :ref:`try
push <Pushing to Try>`, eventually landing
that change on an integration branch.

The details of task-graph generation are documented :ref:`in the source
code itself <TaskCluster Task-Graph Generation>`,
including a some :ref:`quick recipes for common changes <How Tos>`.
