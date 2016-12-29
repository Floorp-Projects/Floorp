Transforms
==========

Many task kinds generate tasks by a process of transforming job descriptions
into task definitions.  The basic operation is simple, although the sequence of
transforms applied for a particular kind may not be!

Overview
--------

To begin, a kind implementation generates a collection of items; see
:doc:`loading`.  The items are simply Python dictionaries, and describe
"semantically" what the resulting task or tasks should do.

The kind also defines a sequence of transformations.  These are applied, in
order, to each item.  Early transforms might apply default values or break
items up into smaller items (for example, chunking a test suite).  Later
transforms rewrite the items entirely, with the final result being a task
definition.

Transform Functions
...................

Each transformation looks like this:

.. code-block::

    @transforms.add
    def transform_an_item(config, items):
        """This transform ..."""  # always a docstring!
        for item in items:
            # ..
            yield item

The ``config`` argument is a Python object containing useful configuration for
the kind, and is a subclass of
:class:`taskgraph.transforms.base.TransformConfig`, which specifies a few of
its attributes.  Kinds may subclass and add additional attributes if necessary.

While most transforms yield one item for each item consumed, this is not always
true: items that are not yielded are effectively filtered out.  Yielding
multiple items for each consumed item implements item duplication; this is how
test chunking is accomplished, for example.

The ``transforms`` object is an instance of
:class:`taskgraph.transforms.base.TransformSequence`, which serves as a simple
mechanism to combine a sequence of transforms into one.

Schemas
.......

The items used in transforms are validated against some simple schemas at
various points in the transformation process.  These schemas accomplish two
things: they provide a place to add comments about the meaning of each field,
and they enforce that the fields are actually used in the documented fashion.

Keyed By
........

Several fields in the input items can be "keyed by" another value in the item.
For example, a test description's chunks may be keyed by ``test-platform``.
In the item, this looks like:

.. code-block:: yaml

    chunks:
        by-test-platform:
            linux64/debug: 12
            linux64/opt: 8
            default: 10

This is a simple but powerful way to encode business rules in the items
provided as input to the transforms, rather than expressing those rules in the
transforms themselves.  If you are implementing a new business rule, prefer
this mode where possible.  The structure is easily resolved to a single value
using :func:`taskgraph.transform.base.get_keyed_by`.

Organization
-------------

Task creation operates broadly in a few phases, with the interfaces of those
stages defined by schemas.  The process begins with the raw data structures
parsed from the YAML files in the kind configuration.  This data can processed
by kind-specific transforms resulting, for test jobs, in a "test description".
For non-test jobs, the next step is a "job description".  These transformations
may also "duplicate" tasks, for example to implement chunking or several
variations of the same task.

In any case, shared transforms then convert this into a "task description",
which the task-generation transforms then convert into a task definition
suitable for ``queue.createTask``.

Test Descriptions
-----------------

Test descriptions specify how to run a unittest or talos run.  They aim to
describe this abstractly, although in many cases the unique nature of
invocation on different platforms leaves a lot of specific behavior in the test
description, divided by ``by-test-platform``.

Test descriptions are validated to conform to the schema in
``taskcluster/taskgraph/transforms/tests.py``.  This schema is extensively
documented and is a the primary reference for anyone modifying tests.

The output of ``tests.py`` is a task description.  Test dependencies are
produced in the form of a dictionary mapping dependency name to task label.

Job Descriptions
----------------

A job description says what to run in the task.  It is a combination of a
``run`` section and all of the fields from a task description.  The run section
has a ``using`` property that defines how this task should be run; for example,
``mozharness`` to run a mozharness script, or ``mach`` to run a mach command.
The remainder of the run section is specific to the run-using implementation.

The effect of a job description is to say "run this thing on this worker".  The
job description must contain enough information about the worker to identify
the workerType and the implementation (docker-worker, generic-worker, etc.).
Alternatively, job descriptions can specify the ``platforms`` field in
conjunction with the  ``by-platform`` key to specify multiple workerTypes and
implementations. Any other task-description information is passed along
verbatim, although it is augmented by the run-using implementation.

The run-using implementations are all located in
``taskcluster/taskgraph/transforms/job``, along with the schemas for their
implementations.  Those well-commented source files are the canonical
documentation for what constitutes a job description, and should be considered
part of the documentation.

following ``run-using`` are available

  * ``hazard``
  * ``mach``
  * ``mozharness``
  * ``run-task``
  * ``spidermonkey`` or ``spidermonkey-package`` or ``spidermonkey-mozjs-crate``
  * ``toolchain-script``


Task Descriptions
-----------------

Every kind needs to create tasks, and all of those tasks have some things in
common.  They all run on one of a small set of worker implementations, each
with their own idiosyncracies.  And they all report to TreeHerder in a similar
way.

The transforms in ``taskcluster/taskgraph/transforms/task.py`` implement
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

More Detail
-----------

The source files provide lots of additional detail, both in the code itself and
in the comments and docstrings.  For the next level of detail beyond this file,
consult the transform source under ``taskcluster/taskgraph/transforms``.
