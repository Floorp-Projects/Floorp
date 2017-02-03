Periodic Taskgraphs
===================

The cron functionality allows in-tree scheduling of task graphs that run
periodically, instead of on a push.

Cron.yml
--------

In the root of the Gecko directory, you will find `.cron.yml`.  This defines
the periodic tasks ("cron jobs") run for Gecko.  Each specifies a name, what to
do, and some parameters to determine when the cron job should occur.

See ``taskcluster/taskgraph/cron/schema.py`` for details on the format and
meaning of this file.

How It Works
------------

The `TaskCluster Hooks Service <https://tools.taskcluster.net/hooks>`_ has a
hook configured for each repository supporting periodic task graphs.  The hook
runs every 15 minutes, and the resulting task is referred to as a "cron task".
That cron task runs `./mach taskgraph cron` in a checkout of the Gecko source
tree.

The mach subcommand reads ``.cron.yml``, then consults the current time
(actually the time the cron task was created, rounded down to the nearest 15
minutes) and creates tasks for any cron jobs scheduled at that time.

Each cron job in ``.cron.yml`` specifies a ``job.using``, corresponding to a
function responsible for creating TaskCluster tasks when the job runs.

Decision Tasks
..............

For ``job.using`` "decision-task", tasks are created based on
``.taskcluster.yml`` just like the decision tasks that result from a push to a
repository.  They run with a distinct ``taskGroupId``, and are free to create
additional tasks comprising a task graph.

Scopes
------

The cron task runs with the sum of all cron job scopes for the given repo.  For
example, for the "sequoia" project, the scope would be
``assume:repo:hg.mozilla.org/projects/sequoia:cron:*``.  Each cron job creates
tasks with scopes for that particular job, by name.  For example, the
``check-frob`` cron job on that repo would run with
``assume:repo:hg.mozilla.org/projects/sequoia:cron:check-frob``.

.. important::

    The individual cron scopes are a useful check to ensure that a job is not
    accidentally doing something it should not, but cannot actually *prevent* a
    job from using any of the scopes afforded to the cron task itself (the
    ``..cron:*`` scope).  This is simply because the cron task runs arbitrary
    code from the repo, and that code can be easily modified to create tasks
    with any scopes that it posesses.
