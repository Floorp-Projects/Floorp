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

When experimenting with local runs of the task-graph generation, it is always
best to find a recent decision task's ``parameters.yml`` file, and modify that
file if necessary, rather than starting from scratch.  This ensures you have a
complete set of parameters.

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

``include_nightly``
   Include nightly builds and tests in the graph.

``owner``
   Email address indicating the person who made the push.  Note that this
   value may be forged and *must not* be relied on for authentication.

``message``
   The commit message

``pushlog_id``
   The ID from the ``hg.mozilla.org`` pushlog

``pushdate``
   The timestamp of the push to the repository that triggered this decision
   task.  Expressed as an integer seconds since the UNIX epoch.

``build_date``
   The timestamp of the build date. Defaults to ``pushdate`` and falls back to present time of
   taskgraph invocation. Expressed as an integer seconds since the UNIX epoch.

``moz_build_date``
   A formatted timestamp of ``build_date``. Expressed as a string with the following
   format: %Y%m%d%H%M%S

Tree Information
----------------

``project``
   Another name for what may otherwise be called tree or branch or
   repository.  This is the unqualified name, such as ``mozilla-central`` or
   ``cedar``.

``level``
   The `SCM level
   <https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/>`_
   associated with this tree.  This dictates the names of resources used in the
   generated tasks, and those tasks will fail if it is incorrect.

Try Configuration
-----------------

``try_mode``
    The mode in which a try push is operating.  This can be one of
    ``"try_task_config"``, ``"try_option_syntax"``, or ``None`` meaning no try
    input was provided.

``try_options``
    The arguments given as try syntax (as a dictionary), or ``None`` if
    ``try_mode`` is not ``try_option_syntax``.

``try_task_config``
    The contents of the ``try_task_config.json`` file, or ``None`` if
    ``try_mode`` is not ``try_task_config``.

Target Set
----------

The "target set" is the set of task labels which must be included in a task
graph.  The task graph generation process will include any tasks required by
those in the target set, recursively.  In a decision task, this set can be
specified programmatically using one of a variety of methods (e.g., parsing try
syntax or reading a project-specific configuration file).

``filters``
    List of filter functions (from ``taskcluster/taskgraph/filter_tasks.py``) to
    apply. This is usually defined internally, as filters are typically
    global.

``target_tasks_method``
    The method to use to determine the target task set.  This is the suffix of
    one of the functions in ``taskcluster/taskgraph/target_tasks.py``.

``include_nightly``
    If true, then nightly tasks are eligible for optimization.

``release_history``
   History of recent releases by platform and locale, used when generating
   partial updates for nightly releases.
   Suitable contents can be generated with ``mach release-history``,
   which will print to the console by default.

Optimization
------------

``optimize_target_tasks``
    If true, then target tasks are eligible for optimization.

``do_not_optimize``
   Specify tasks to not optimize out of the graph. This is a list of labels.
   Any tasks in the graph matching one of the labels will not be optimized out
   of the graph.

``existing_tasks``
   Specify tasks to optimize out of the graph. This is a dictionary of label to taskId.
   Any tasks in the graph matching one of the labels will use the previously-run
   taskId rather than submitting a new task.

Release Promotion
-----------------

``build_number``
   Specify the release promotion build number.

``next_version``
   Specify the next version for version bump tasks.

``desktop_release_type``
   The type of desktop release being promoted. One of "beta", "devedition", "esr", "rc",
   or "release".

 ``release_eta``
   The time and date when a release is scheduled to live. This value is passed to Balrog.

Comm Push Information
---------------------

These parameters correspond to the repository and revision of the comm-central
repository to checkout. Their meaning is the same as the corresponding
parameters for the gecko repository above. They are optional, but if any of
them are specified, they must all be specified.

``comm_base_repository``
``comm_head_repository``
``comm_head_rev``
``comm_head_ref``
