Transforms
==========

Many task kinds generate tasks by a process of transforming job descriptions
into task definitions.  The basic operation is simple, although the sequence of
transforms applied for a particular kind may not be!

Overview
--------

To begin, a kind implementation generates a collection of items.  For example,
the test kind implementation generates a list of tests to run for each matching
build, representing each as a test description.  The items are simply Python
dictionaries.

The kind also defines a sequence of transformations.  These are applied, in
order, to each item.  Early transforms might apply default values or break
items up into smaller items (for example, chunking a test suite).  Later
transforms rewrite the items entirely, with the final result being a task
definition.

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
-------

The items used in transforms are validated against some simple schemas at
various points in the transformation process.  These schemas accomplish two
things: they provide a place to add comments about the meaning of each field,
and they enforce that the fields are actually used in the documented fashion.

Keyed By
--------

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

Task-Generation Transforms
--------------------------

Every kind needs to create tasks, and all of those tasks have some things in
common.  They all run on one of a small set of worker implementations, each
with their own idiosyncracies.  And they all report to TreeHerder in a similar
way.

The transforms in ``taskcluster/taskgraph/transforms/make_task.py`` implement
this common functionality.  They expect a "task description", and produce a
task definition.  The schema for a task description is defined at the top of
``make_task.py``, with copious comments.  The parts of the task description
that are specific to a worker implementation are isolated in a ``worker``
object which has an ``implementation`` property naming the worker
implementation.  Thus the transforms that produce a task description must be
aware of the worker implementation to be used, but need not be aware of the
details of its payload format.

The result is a dictionary with keys ``label``, ``attributes``, ``task``, and
``dependencies``, with the latter having the same format as the input
dependencies.

These transforms assign names to treeherder groups using an internal list of
group names.  Feel free to add additional groups to this list as necessary.

Test Transforms
---------------

The transforms configured for test kinds proceed as follows, based on
configuration in ``kind.yml``:

 * The test description is validated to conform to the schema in
   ``taskcluster/taskgraph/transforms/tests/test_description.py``.  This schema
   is extensively documented and is a the primary reference for anyone
   modifying tests.

 * Kind-specific transformations are applied.  These may apply default
   settings, split tests (e.g., one to run with feature X enabled, one with it
   disabled), or apply across-the-board business rules such as "all desktop
   debug test platforms should have a max-run-time of 5400s".

 * Transformations generic to all tests are applied.  These apply policies
   which apply to multiple kinds, e.g., for treeherder tiers.  This is also the
   place where most values which differ based on platform are resolved, and
   where chunked tests are split out into a test per chunk.

 * The test is again validated against the same schema.  At this point it is
   still a test description, just with defaults and policies applied, and
   per-platform options resolved.  So transforms up to this point do not modify
   the "shape" of the test description, and are still governed by the schema in
   ``test_description.py``.

 * The ``taskgraph.transforms.tests.make_task_description:transforms`` then
   take the test description and create a *task* description.  This transform
   embodies the specifics of how test runs work: invoking mozharness, various
   worker options, and so on.

 * Finally, the ``taskgraph.transforms.make_task:transforms``, described above
   under "Task-Generation Transforms", are applied.

Test dependencies are produced in the form of a dictionary mapping dependency
name to task label.
