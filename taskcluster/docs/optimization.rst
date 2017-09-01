Optimization
============

The objective of optimization to remove as many tasks from the graph as
possible, as efficiently as possible, thereby delivering useful results as
quickly as possible.  For example, ideally if only a test script is modified in
a push, then the resulting graph contains only the corresponding test suite
task.

A task is said to be "optimized" when it is either replaced with an equivalent,
already-existing task, or dropped from the graph entirely.

Optimization Functions
----------------------

During the optimization phase of task-graph generation, each task is optimized
in post-order, meaning that each task's dependencies will be optimized before
the task itself is optimized.

Each task has a ``task.optimizations`` property describing the optimization
methods that apply.  Each is specified as a list of method and arguments. For
example::

    task.optimizations = [
        ['seta'],
        ['skip-unless-changed', ['js/**', 'tests/**']],
    ]

These methods are defined in ``taskcluster/taskgraph/optimize.py``.  They are
applied in order, and the first to return a success value causes the task to
be optimized.

Each method can return either a taskId (indicating that the given task can be
replaced) or indicate that the task can be optimized away. If a task on which
others depend is optimized away, task-graph generation will fail.

Optimizing Target Tasks
-----------------------

In some cases, such as try pushes, tasks in the target task set have been
explicitly requested and are thus excluded from optimization. In other cases,
the target task set is almost the entire task graph, so targetted tasks are
considered for optimization.  This behavior is controlled with the
``optimize_target_tasks`` parameter.
