Taskcluster Mach commands
=========================

A number of mach subcommands are available aside from ``mach taskgraph
decision`` to make this complex system more accessible to those trying to
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

``mach taskgraph morphed``
   Get the morphed task graph

See :doc:`how-tos` for further practical tips on debugging task-graph mechanics
locally.

Parameters
----------

Each of these commands takes an optional ``--parameters`` argument giving a file
with parameters to guide the graph generation.  The decision task helpfully
produces such a file on every run, and that is generally the easiest way to get
a parameter file.  The parameter keys and values are described in
:doc:`parameters`; using that information, you may modify an existing
``parameters.yml`` or create your own.  The ``--parameters`` option can also
take the following forms:

``project=<project>``
   Fetch the parameters from the latest push on that project
``task-id=<task-id>``
   Fetch the parameters from the given decision task id

If not specified, parameters will default to ``project=mozilla-central``.

Taskgraph JSON Format
---------------------
By default, the above commands will only output a list of tasks. Use `-J` flag
to output full task definitions. For example:

.. code-block:: shell

    $ ./mach taskgraph optimized -J


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

An alternate way of searching the output of ``mach taskgraph`` is
`gron <https://github.com/tomnomnom/gron>`_, which converts json into a format
that's easily searched with ``grep``

.. code-block:: shell

    gron taskgraph.json | grep -E 'test.*machine.platform = "linux64";'
    ./mach taskgraph --json | gron | grep ...


Diffing Taskgraphs
------------------

A common use case for running ``./mach taskgraph`` is to examine what changed
in the taskgraph from one revision to the next. The ``--diff`` flag can
facilitate this by automatically updating to the base revision of your stack
(or a specified revision) and generating the taskgraph there. It will then
update back to the current revision, generate the taskgraph again and produce a
diff of the two. It can work with both labels and JSON (``-J``). All flags
(such as ``--target-kind``, ``--parameters`` or ``--target-tasks-filter``) will
apply to both graphs so can therefore be used in conjunction with ``--diff``.


For example:

.. code-block:: shell

    ./mach taskgraph target --target-kind test --fast -J --diff

The above will display a diff of the current taskgraph compared to the base
revision of your stack. To compare against arbitrary revisions you can use
``--diff <specifier>``. E.g:

.. code-block:: shell

    ./mach taskgraph target --diff .~1    # hg
    ./mach taskgraph target --dif HEAD~1  # git
