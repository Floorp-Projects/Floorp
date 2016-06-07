==========
Parameters
==========

Task-graph generation takes a collection of parameters as input, in the form of
a JSON or YAML file.

During decision-task processing, some of these parameters are supplied on the
command line or by environment variables.  The decision task helpfully produces
a full parameters file as one of its output artifacts.  The other ``mach
taskgraph`` commands can take this file as input.  This can be very helpful
when working on a change to the task graph.

The properties of the parameters object are described here, divided rougly by
topic.

Push Information
----------------

``base_repository``
   The repository from which to do an initial clone, utilizing any available
   caching.

``head_repository``
   The repository containing the changeset to be built.  This may differ from
   ``base_repository`` in cases where ``base_repository`` is likely to be cached
   and only a few additional commits are needed from ``head_repository``.

``head_rev``
   The revision to check out; this can be a short revision string

``head_ref``
   For Mercurial repositories, this is the same as ``head_rev``.  For
   git repositories, which do not allow pulling explicit revisions, this gives
   the symbolic ref containing ``head_rev`` that should be pulled from
   ``head_repository``.

``revision_hash``
   The full-length revision string

``owner``
   Email address indicating the person who made the push.  Note that this
   value may be forged and *must not* be relied on for authentication.

``message``
   The commit message

``pushlog_id``
   The ID from the ``hg.mozilla.org`` pushlog

Tree Information
----------------

``project``
   Another name for what may otherwise be called tree or branch or
   repository.  This is the unqualified name, such as ``mozilla-central`` or
   ``cedar``.

``level``
   The SCM level associated with this tree.  This dictates the names
   of resources used in the generated tasks, and those tasks will fail if it
   is incorrect.

Target Set
----------

The "target set" is the set of task labels which must be included in a task
graph.  The task graph generation process will include any tasks required by
those in the target set, recursively.  In a decision task, this set can be
specified programmatically using one of a variety of methods (e.g., parsing try
syntax or reading a project-specific configuration file).

The decision task writes its task set to the ``target_tasks.json`` artifact,
and this can be copied into ``parameters.target_tasks`` and
``parameters.target_tasks_method`` set to ``"from_parameters"`` for debugging
with other ``mach taskgraph`` commands.

``target_tasks_method``
   (optional) The method to use to determine the target task set.  This is the
   suffix of one of the functions in ``tascluster/taskgraph/target_tasks.py``.
   If omitted, all tasks are targeted.

``target_tasks``
   (optional) The target set method ``from_parameters`` reads the target set, as
   a list of task labels, from this parameter.

``optimize_target_tasks``
   (optional; default True) If true, then target tasks are eligible for
   optimization.
