Loading Tasks
=============

The full task graph generation involves creating tasks for each kind.  Kinds
are ordered to satisfy ``kind-dependencies``, and then the ``implementation``
specified in ``kind.yml`` is used to load the tasks for that kind.

Specifically, the class's ``load_tasks`` class method is called, and returns a
list of new ``Task`` instances.

TransformTask
-------------

Most kinds generate their tasks by starting with a set of items describing the
jobs that should be performed and transforming them into task definitions.
This is the familiar ``transforms`` key in ``kind.yml`` and is further
documented in :doc:`transforms`.

Such kinds generally specify their tasks in a common format: either based on a
``jobs`` property in ``kind.yml``, or on YAML files listed in ``jobs-from``.
This is handled by the ``TransformTask`` class in
``taskcluster/taskgraph/task/transform.py``.

For kinds producing tasks that depend on other tasks -- for example, signing
tasks depend on build tasks -- ``TransformTask`` has a ``get_inputs`` method
that can be overridden in subclasses and written to return a set of items based
on tasks that already exist.  You can see a nice example of this behavior in
``taskcluster/taskgraph/task/post_build.py``.

For more information on how all of this works, consult the docstrings and
comments in the source code itself.

Try option syntax
-----------------

The ``parse-commit`` optional field specified in ``kind.yml`` links to a
function to parse the command line options in the ``--message`` mach parameter.
Currently, the only valid value is ``taskgraph.try_option_syntax:parse_message``.
The parsed arguments are stored in ``config.config['args']``, it corresponds
to the same object returned by ``parse_args`` from ``argparse`` Python module.
