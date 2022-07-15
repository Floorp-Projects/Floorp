Task Transforms
===============

.. note::

   These transforms are currently duplicated by standalone Taskgraph
   and will likely be refactored / removed at a later date.

Every kind needs to create tasks, and all of those tasks have some things in
common.  They all run on one of a small set of worker implementations, each
with their own idiosyncrasies.  And they all report to TreeHerder in a similar
way.

The transforms in ``taskcluster/gecko_taskgraph/transforms/task.py`` implement
this common functionality.  They expect a "task description", and produce a
task definition.  The schema for a task description is defined at the top of
``task.py``, with copious comments.  Go forth and read it now!

In general, the task-description transforms handle functionality that is common
to all Gecko tasks.  While the schema is the definitive reference, the
functionality includes:

* TreeHerder metadata

* Build index routes

* Information about the projects on which this task should run

* Optimizations

* Defaults for ``expires-after`` and and ``deadline-after``, based on project

* Worker configuration

The parts of the task description that are specific to a worker implementation
are isolated in a ``task_description['worker']`` object which has an
``implementation`` property naming the worker implementation.  Each worker
implementation has its own section of the schema describing the fields it
expects.  Thus the transforms that produce a task description must be aware of
the worker implementation to be used, but need not be aware of the details of
its payload format.

The ``task.py`` file also contains a dictionary mapping treeherder groups to
group names using an internal list of group names.  Feel free to add additional
groups to this list as necessary.
