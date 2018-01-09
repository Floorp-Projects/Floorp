Optimization
============

The objective of optimization to remove as many tasks from the graph as
possible, as efficiently as possible, thereby delivering useful results as
quickly as possible. For example, ideally if only a test script is modified in
a push, then the resulting graph contains only the corresponding test suite
task.

A task is said to be "optimized" when it is either replaced with an equivalent,
already-existing task, or dropped from the graph entirely.

Optimization Strategies
-----------------------

Each task has a single named optimization strategy, and can provide an argument
to that strategy. Each strategy is defined as an ``OptimizationStrategy``
instance in ``taskcluster/taskgraph/optimization.py``.

Each task has a ``task.optimization`` property describing the optimization
strategy that applies, specified as a dictionary mapping strategy to argument. For
example::

    task.optimization = {'skip-unless-changed': ['js/**', 'tests/**']}

Strategy implementations are shared across all tasks, so they may cache
commonly-used information as instance variables.

Optimizing Target Tasks
-----------------------

In some cases, such as try pushes, tasks in the target task set have been
explicitly requested and are thus excluded from optimization. In other cases,
the target task set is almost the entire task graph, so targetted tasks are
considered for optimization. This behavior is controlled with the
``optimize_target_tasks`` parameter.

.. note:

    Because it is a mix of "what the push author wanted" and "what should run
    when necessary", try pushes with the old option syntax (``-b do -p all``,
    etc.) *do* optimize target tasks.  This can cause unexpected results when
    requested jobs are optimized away.  If those jobs were actually necessary,
    then a try push with ``try_task_config.json`` is the solution.

More Information
----------------

.. toctree::

    optimization-process
    optimization-schedules
