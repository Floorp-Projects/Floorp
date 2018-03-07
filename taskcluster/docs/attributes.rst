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
requirements like "only run this on inbound" get implemented.  These are
either project names or the aliases

 * `integration` -- integration repositories (autoland, inbound, etc)
 * `trunk` -- integration repositories plus mozilla-central
 * `release` -- release repositories including mozilla-central
 * `all` -- everywhere (the default)

For try, this attribute applies only if ``-p all`` is specified.  All jobs can
be specified by name regardless of ``run_on_projects``.

If ``run_on_projects`` is set to an empty list, then the task will not run
anywhere, unless its build platform is specified explicitly in try syntax.

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

unittest_flavor
===============

If a unittest suite has subdivisions, those are represented as flavors.  Not
all suites have flavors, in which case this attribute should be set to match
the suite.  Examples: ``mochitest-devtools-chrome-chunked`` or ``a11y``.

unittest_try_name
=================

This is the name used to refer to a unit test via try syntax.  It
may not match either of ``unittest_suite`` or ``unittest_flavor``.

talos_try_name
==============

This is the name used to refer to a talos job via try syntax.

job_try_name
============

This is the name used to refer to a "job" via try syntax (``-j``).  Note that for
some kinds, ``-j`` also matches against ``build_platform``.

test_chunk
==========

This is the chunk number of a chunked test suite (talos or unittest).  Note
that this is a string!

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

all_locales
===========

For the ``l10n`` and ``nightly-l10n`` kinds, this attribute contains the list
of relevant locales for the platform.

all_locales_with_changesets
===========================

Contains a dict of l10n changesets, mapped by locales (same as in ``all_locales``).

l10n_chunk
==========
For the ``l10n`` and ``nightly-l10n`` kinds, this attribute contains the chunk
number of the job. Note that this is a string!

chunk_locales
=============
For the ``l10n`` and ``nightly-l10n`` kinds, this attribute contains an array of
the individual locales this chunk is responsible for processing.

locale
======
For jobs that operate on only one locale, we set the attribute ``locale`` to the
specific locale involved. Currently this is only in l10n versions of the
``beetmover`` and ``balrog`` kinds.

signed
======
Signals that the output of this task contains signed artifacts.

repackage_type
==============
This is the type of repackage. Can be ``repackage`` or
``repackage_signing``.

toolchain-artifact
==================
For toolchain jobs, this is the path to the artifact for that toolchain.

toolchain-alias
===============
For toolchain jobs, this optionally gives an alias that can be used instead of the
real toolchain job name in the toolchains list for build jobs.

always_target
=============

Tasks with this attribute will be included in the ``target_task_graph`` regardless
of any target task filtering that occurs. When a task is included in this manner
(i.e it otherwise would have been filtered out), it will be considered for
optimization even if the ``optimize_target_tasks`` parameter is False.

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
overrideable per-task.
