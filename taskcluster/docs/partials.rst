Partial Update Generation
=========================

.. _overview

Overview
--------

Windows, Mac and Linux releases have partial updates, to reduce
the file size end-users have to download in order to receive new
versions. These are created using a docker image, some Python,
``mbsdiff``, and the tools in ``tools/update-packaging``

The task has been called 'Funsize' for quite some time. This might
make sense depending on what brands of chocolate bar are available
near you.

.. _how the task works

How the Task Works
------------------

Funsize uses a docker image that's built in-tree, named funsize-update-generator.
The image contains some Python to examine the task definition and determine
what needs to be done, but it downloads tools like ``mar`` and ``mbsdiff``
from either locations specified in the task definition, or default mozilla-central
locations.

The 'extra' section of the task definition contains most of the payload, under
the 'funsize' key. In here is a list of partials that this specific task will
generate, and each entry includes the earlier (or 'from') version, and the most
recent (or 'to') version, which for most releases will likely be a taskcluster
artifact.

.. code-block:: json

    {
       "to_mar": "https://queue.taskcluster.net/v1/task/EWtBFqVuT-WqG3tGLxWhmA/artifacts/public/build/ach/target.complete.mar",
       "product": "Firefox",
       "dest_mar": "target-60.0b8.partial.mar",
       "locale": "ach",
       "from_mar": "http://archive.mozilla.org/pub/firefox/candidates/60.0b8-candidates/build1/update/linux-i686/ach/firefox-60.0b8.complete.mar",
       "update_number": 2,
       "platform": "linux32",
       "previousVersion": "60.0b8",
       "previousBuildNumber": "1",
       "branch": "mozilla-beta"
     },

The 'update number' indicates how many released versions there are between 'to' and the current 'from'.
For example, if we are building a partial update for the current nightly from the previous one, the update
number will be 1. For the release before that, it will be 2. This lets us use generic output artifact
names that we can rename in the later ``beetmover`` tasks.

Inside the task, for each partial it has been told to generate, it will download, unpack and virus
scan the 'from_mar' and 'to_mar', download the tools, and run ``make_incremental_update.sh`` from
``tools/update-packaging``.

If a scope is given for a set of temporary S3 credentials, the task will use a caching script,
to allow re-use of the diffs made for larger files. Some of the larger files are not localised,
and this allows us to save a lot of compute time.

For Releases
------------

Partials are made as part of the ``promote`` task group. The previous
versions used to create the update are specified in ship-it by
Release Management.

.. _data and metrics

Data About Partials
-------------------

Some metrics are collected in Datadog_ about partial update tasks.
The prefix used is ``releng.releases.partials``, so the relevant metric names
will all start with that.

Some dashboards in Datadog are public, some require a login. If you need
access, file a bug under 'Cloud Services :: Operations: Metrics/Monitoring'

Some examples of potentially useful metrics:

* ``releng.releases.partials.partial_mar_size`` (tagged with branch, platform and update-number)
* ``releng.releases.partials.task_duration`` - the time the task took, running partial generation concurrently.
* ``releng.releases.partials.generate_partial.time`` - the time taken to make one partial update

.. _nightly partials

Nightly Partials
----------------

Since nightly releases don't appear in ship-it, the partials to create
are determined in the decision task. This was controversial, and so here
are the assumptions and reasons, so that when an alternative solution is
discovered, we can assess it in context:

1. Balrog is the source of truth for previous nightly releases.
2. Re-running a task should produce the same results.
3. A task's input and output should be specified in the definition.
4. A task transform should avoid external dependencies. This is to
   increase the number of scenarios in which 'mach taskgraph' works.
5. A task graph doesn't explicitly know that it's intended for nightlies,
   only that specific tasks are only present for nightly.
6. The decision task is explicitly told that its target is nightly
   using the target-tasks-method argument.

a. From 2 and 3, this means that the partials task itself cannot query
   balrog for the history, as it may get different results when re-run,
   and hides the inputs and outputs from the task definition.
b. From 4, anything run by 'mach taskgraph' is an inappropriate place
   to query Balrog, even if it results in a repeatable task graph.
c. Since these restrictions don't apply to the decision task, and given
   6, we can query Balrog in the decision task if the target-tasks-method
   given contains 'nightly', such as 'nightly_desktop' or 'nightly_linux'

Using the decision task involves making fewer, larger queries to Balrog,
and storing the results for task graph regeneration and later audit. At
the moment this data is stored in the ``parameters`` under the label
``release_history``, since the parameters are an existing method for
passing data to the task transforms, but a case could be made
for adding a separate store, as it's a significantly larger number of
records than anything else in the parameters.

Nightly Partials and Beetmover
------------------------------

A release for a specific platform and locale may not have a history of
prior releases that can be used to build partial updates. This could be
for a variety of reasons, such as a new locale, or a hiatus in nightly
releases creating too long a gap in the history.

This means that the ``partials`` and ``partials-signing`` tasks may have
nothing to do for a platform and locale. If this is true, then the tasks
are filtered out in the ``transform``.

This does mean that the downstream task, ``beetmover-repackage`` can not
rely on the ``partials-signing`` task existing. It depends on both the
``partials-signing`` and ``repackage-signing`` task, and chooses which
to depend on in the transform.

If there is a history in the ``parameters`` ``release_history`` section
then ``beetmover-repackage`` will depend on ``partials-signing``.
Otherwise, it will depend on ``repackage-signing``.

This is not ideal, as it results in unclear logic in the task graph
generation. It will be improved.

.. _Datadog: https://app.datadoghq.com/metric/explorer
