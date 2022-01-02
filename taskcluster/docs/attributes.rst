===============
Task Attributes
===============

Tasks can be filtered, for example to support "try" pushes which only perform a
subset of the task graph or to link dependent tasks.  This filtering is the
difference between a full task graph and a target task graph.

Filtering takes place on the basis of attributes.  Each task has a dictionary
of attributes and filters over those attributes can be expressed in Python.  A
task may not have a value for every attribute.

The attributes, and acceptable values, are defined here.  In general, attribute
names and values are the short, lower-case form, with underscores.

kind
====

A task's ``kind`` attribute gives the name of the kind that generated it, e.g.,
``build`` or ``spidermonkey``.

run_on_projects
===============

The projects where this task should be in the target task set.  This is how
requirements like "only run this on autoland" get implemented.

.. note::

    Please use this configuration. Running a job for all projects can quickly add up
    in term of cost while not providing any value for some projects.

`run-on-projects` can use either aliases or project names.

These are the aliases:

 * `integration` -- integration repository (autoland)
 * `trunk` -- integration repository plus mozilla-central
 * `release` -- release repositories (beta, release, esr) including mozilla-central
 * `all` -- everywhere (the default)

Project names are the repositories.  They can be:

* `autoland`
* `mozilla-central`
* `mozilla-beta`
* `mozilla-release`
* `mozilla-esr91`
* ... A partial list can be found in taskcluster/gecko_taskgraph/util/attributes.py

For try, this attribute applies only if ``-p all`` is specified.  All jobs can
be specified by name regardless of ``run_on_projects``.

If ``run_on_projects`` is set to an empty list, then the task will not run
anywhere, unless its build platform is specified explicitly in try syntax.


.. note::

    As `try` pushes don't use filter_for_projects by design, there isn't a way
    to define that a task will run on `try`.


.. note::

    A given task `[taskA]` may not respect `run-on-projects` if there another task
    `[taskB]` which is scheduled to run (such as via run-on-projects) which depends it
    `[taskA]`. Because by nature of `TaskB` running we must run `TaskA`.

    See `bug 1640603 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640603#c5>`_ as example.

run_on_hg_branches
==================

On a given project, the mercurial branch where this task should be in the target
task set.  This is how requirements like "only run this RELBRANCH" get implemented.
These are either the regular expression of a branch (e.g.: ``GECKOVIEW_\d+_RELBRANCH``)
or the following alias:

 * `all` -- everywhere (the default)

Like ``run_on_projects``, the same behavior applies if it is set to an empty list.

task_duplicates
===============

This is used to indicate that we want multiple copies of the task created.
This feature is used to track down intermittent job failures.

If this value is set to N, the task-creation machinery will create a total of N
copies of the task.  Only the first copy will be included in the taskgraph
output artifacts, although all tasks will be contained in the same taskGroup.

While most attributes are considered read-only, target task methods may alter
this attribute of tasks they include in the target set.

build_platform
==============

The build platform defines the platform for which the binary was built.  It is
set for both build and test jobs, although test jobs may have a different
``test_platform``.

build_type
==========

The type of build being performed.  This is a subdivision of ``build_platform``,
used for different kinds of builds that target the same platform.  Values are

 * ``debug``
 * ``opt``

test_platform
=============

The test platform defines the platform on which tests are run.  It is only
defined for test jobs and may differ from ``build_platform`` when the same binary
is tested on several platforms (for example, on several versions of Windows).
This applies for both talos and unit tests.

Unlike build_platform, the test platform is represented in a slash-separated
format, e.g., ``linux64/opt``.

unittest_suite
==============

This is the unit test suite being run in a unit test task.  For example,
``mochitest`` or ``cppunittest``.

unittest_category
=================

This is the high-level category of test the suite corresponds to. This is
usually the test harness used to run the suite.

unittest_try_name
=================

This is the name used to refer to a unit test via try syntax.  It
may not match ``unittest_suite``.

unittest_variant
================

The configuration variant the test suite is running with. If set, this usually
means the tests are running with a special pref enabled. These are defined in
``taskgraph.transforms.test.TEST_VARIANTS``.

talos_try_name
==============

This is the name used to refer to a talos job via try syntax.

raptor_try_name
===============

This is the name used to refer to a raptor job via try syntax.

job_try_name
============

This is the name used to refer to a "job" via try syntax (``-j``).  Note that for
some kinds, ``-j`` also matches against ``build_platform``.

test_chunk
==========

This is the chunk number of a chunked test suite. Note that this is a string!

test_manifests
==============

A list of the test manifests that run in this task.

e10s
====

For test suites which distinguish whether they run with or without e10s, this
boolean value identifies this particular run.

image_name
==========

For the ``docker_image`` kind, this attribute contains the docker image name.

nightly
=======

Signals whether the task is part of a nightly graph. Useful when filtering
out nightly tasks from full task set at target stage.

shippable
=========
Signals whether the task is considered "shippable", that it should get signed and is ok to
be used for nightlies or releases.

all_locales
===========

For the ``l10n`` and ``shippable-l10n`` kinds, this attribute contains the list
of relevant locales for the platform.

all_locales_with_changesets
===========================

Contains a dict of l10n changesets, mapped by locales (same as in ``all_locales``).

l10n_chunk
==========
For the ``l10n`` and ``shippable-l10n`` kinds, this attribute contains the chunk
number of the job. Note that this is a string!

chunk_locales
=============
For the ``l10n`` and ``shippable-l10n`` kinds, this attribute contains an array of
the individual locales this chunk is responsible for processing.

locale
======
For jobs that operate on only one locale, we set the attribute ``locale`` to the
specific locale involved. Currently this is only in l10n versions of the
``beetmover`` and ``balrog`` kinds.

signed
======
Signals that the output of this task contains signed artifacts.

stub-installer
==============
Signals to the build system that this build is expected to have a stub installer
present, and informs followon tasks to expect it.

repackage_type
==============
This is the type of repackage. Can be ``repackage`` or
``repackage_signing``.

fetch-artifact
==============

For fetch jobs, this is the path to the artifact for that fetch operation.

fetch-alias
===========
An alias that can be used instead of the real fetch job name in fetch
stanzas for jobs.

toolchain-artifact
==================
For toolchain jobs, this is the path to the artifact for that toolchain.

toolchain-alias
===============
An alias that can be used instead of the real toolchain job name in fetch
stanzas for jobs.

toolchain-env
=============
Extra environment variables that will be set on the worker when fetching this
toolchain.

always_target
=============

Tasks with this attribute will be included in the ``target_task_graph`` if
``parameters["tasks_for"]`` is ``hg-push``, regardless of any target task
filtering that occurs. When a task is included in this manner (i.e it otherwise
would have been filtered out), it will be considered for optimization even if
the ``optimize_target_tasks`` parameter is False.

This is meant to be used for tasks which a developer would almost always want to
run. Typically these tasks will be short running and have a high risk of causing
a backout. For example ``lint`` or ``python-unittest`` tasks.

shipping_product
================
For release promotion jobs, this is the product we are shipping.

shipping_phase
==============
For release promotion jobs, this is the shipping phase (build, promote, push, ship).
During the build phase, we build and sign shippable builds. During the promote phase,
we generate l10n repacks and push to the candidates directory. During the push phase,
we push to the releases directory. During the ship phase, we update bouncer, push to
Google Play, version bump, mark as shipped in ship-it.

Using the "snowman model", we depend on previous graphs if they're defined. So if we
ask for a ``push`` (the head of the snowman) and point at the body and base, we only
build the head. If we don't point at the body and base, we build the whole snowman
(build, promote, push).

artifact_prefix
===============
Most taskcluster artifacts are public, so we've hardcoded ``public/build`` in a
lot of places. To support private artifacts, we've moved this to the
``artifact_prefix`` attribute. It will default to ``public/build`` but will be
overridable per-task.

artifact_map
===============
For beetmover jobs, this indicates which yaml file should be used to
generate the upstream artifacts and payload instructions to the task.

batch
=====
Used by `perftest` to indicates that a task can be run as a batch.


enable-full-crashsymbols
========================
In automation, full crashsymbol package generation is normally disabled.  For
build kinds where the full crashsymbols should be enabled, set this attribute
to True. The full symbol packages will then be generated and uploaded on
release branches and on try.

skip-upload-crashsymbols
========================
Shippable/nightly builds are normally required to set enable-full-crashsymbols,
but in some limited corner cases (universal builds), that is not wanted, because
the symbols are uploaded independently already.

cron
====
Indicates that a task is meant to be run via cron tasks, and should not be run
on push.

cached_task
===========
Some tasks generate artifacts that are cached between pushes. This is a
dictionary with the type and name of the cache, and the unique string used to
identify the current version of the artifacts. See :py:mod:`taskgraph.util.cached_task`.

.. code:: yaml

   cached_task:
       digest: 66dfc2204600b48d92a049b6a18b83972bb9a92f9504c06608a9c20eb4c9d8ae
       name: debian7-base
       type: docker-images.v2

eager_indexes
=============
A list of strings of indexes to populate before the task ever completes. Some tasks (e.g. cached tasks) we
want to exist in the index before they even run/complete. Our current use is to allow us to depend on an
unfinished cached task in future pushes. This avoids extra overhead from multiple tasks running, and
can allow us to have our results in just a bit earlier.

required_signoffs
=================
A list of release signoffs that this kind requires, should the release also
require these signoffs. For example, ``mar-signing`` signoffs may be required
by some releases in the future; for any releases that require ``mar-signing``
signoffs, the kinds that also require that signoff are marked with this
attribute.

update-channel
==============
The update channel the build is configured to use.

mar-channel-id
==============
The mar-channel-id the build is configured to use.

accepted-mar-channel-ids
========================
The mar-channel-ids this build will accept updates to. It should usually be the same as
the value mar_channel_id.  If more than one ID is needed, then you should use a
comma separated list of values.

openh264_rev
============
Only used for openh264 plugin builds, used to signify the revision (and thus inform artifact name) of the given build.

code-review
===========
If a task set this boolean attribute to `true`, it will be processed by the code
review bot, the task will ran for every new Phabricator diff.
Any supported and detected issue will be automatically reported on the
Phabricator revision.

resource-monitor
================
If a task set this boolean attribute to `true`, it will collect CPU, memory, and
- if available - Disk and Network IO by running the resource-monitor utility,
provided through fetches.

retrigger
=========
Whether the task can be retriggered, or if it needs to be re-run.

disable-push-apk
================
Some GeckoView-only Android tasks produce APKs that shouldn't be
pushed to the Google Play Store.  Set this to ``true`` to disable
pushing.

disable-build-signing
=====================
Some GeckoView-only tasks produce APKs, but not APKs that should be
signed.  Set this to ``true`` to disable APK signing.

enable-build-signing
====================
We enable build-signing for ``shippable``, ``nightly``, and ``enable-build-signing`` tasks.

run-visual-metrics
==================
If set to true, will run the visual metrics task on the provided
video files.

skip-verify-test-packaging
==========================
If set to true, this task will not be checked to see that
MOZ_AUTOMATION_PACKAGE_TESTS is set correctly based on whether or not the task
has dependent tests. This should only be used in very unique situations, such
as Windows AArch64 builds that copy test packages between build tasks.

geckodriver
===========
If non-empty, declares that the (toolchain) task is a `geckodriver`
task that produces a binary that should be signed.

rebuild-on-release
==================
If true, the digest for this task will also depend on if the branch is a
release branch.  This will cause tasks like toolchains to be rebuilt as they
move from e.g. autoland to mozilla-central.

local-toolchain
===============
This toolchain is used for local development, so should be built on trunk, even
if it does not have any in-graph consumers.

artifact-build
==============

This build is an artifact build.

This deliberately excludes builds that are implemented using the artifact build
machinery, but are not primarily intended to short-circuit build time. In
particular the Windows aarch64 builds are not marked this way.

supports-artifact-builds
========================

If false, the task requires a compiled build and will not work with artifact builds.
