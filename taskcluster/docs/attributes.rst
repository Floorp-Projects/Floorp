===============
Task Attributes
===============

Tasks can be filtered, for example to support "try" pushes which only perform a
subset of the task graph or to link dependent tasks.  This filtering is the
difference between a full task graph and a target task graph.

Filtering takes place on the basis of attributes.  Each task has a dictionary
of attributes (all strings), and a filter is an arbitrary expression over those
attributes.  A task may not have a value for every attribute.

The attributes, and acceptable values, are defined here.  In general, attribute
names and values are the short, lower-case form, with underscores.

kind
====

A task's ``kind`` attribute gives the name of the kind that generated it, e.g.,
``build`` or ``legacy``.

build_platform
==============

The build platform defines the platform for which the binary was built.  It is
set for both build and test jobs, although test jobs may have a different
``test_platform``.

test_platform
=============

The test platform defines the platform on which tests are run.  It is only
defined for test jobs and may differ from ``build_platform`` when the same binary
is tested on several platforms (for example, on several versions of Windows).
This applies for both talos and unit tests.

build_type
==========

The type of build being performed.  This is a subdivision of ``build_platform``,
used for different kinds of builds that target the same platform.  Values are

 * ``debug``
 * ``opt``

unittest_suite
==============

This is the unit test suite being run in a unit test task.  For example,
``mochitest`` or ``cppunittest``.

unittest_flavor
===============

If a unittest suite has subdivisions, those are represented as flavors.  Not
all suites have flavors, in which case this attribute should be omitted (but is
sometimes set to match the suite).  Examples:
``mochitest-devtools-chrome-chunked`` or ``a11y``.

unittest_try_name
=================

(deprecated) This is the name used to refer to a unit test via try syntax.  It
may not match either of ``unittest_suite`` or ``unittest_flavor``.

test_chunk
==========

This is the chunk number of a chunked test suite (talos or unittest).  Note
that this is a string!

legacy_kind
===========

(deprecated) The kind of task as created by the legacy kind.  This is valid
only for the ``legacy`` kind.  One of ``build``, ``unittest,`` ``post_build``,
or ``job``.

job
===

(deprecated) The name of the job (corresponding to a ``-j`` option or the name
of a post-build job).  This is valid only for the ``legacy`` kind.

post_build
==========

(deprecated) The name of the post-build activity.  This is valid only for the
``legacy`` kind.

