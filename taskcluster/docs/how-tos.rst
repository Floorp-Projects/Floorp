How Tos
=======

All of this equipment is here to help you get your work done more efficiently.
However, learning how task-graphs are generated is probably not the work you
are interested in doing.  This section should help you accomplish some of the
more common changes to the task graph with minimal fuss.

.. important::

    If you cannot accomplish what you need with the information provided here,
    please consider whether you can achieve your goal in a different way.
    Perhaps something simpler would cost a bit more in compute time, but save
    the much more expensive resource of developers' mental bandwidth.
    Task-graph generation is already complex enough!

    If you want to proceed, you may need to delve into the implementation of
    task-graph generation.  The documentation and code are designed to help, as
    are the authors - ``hg blame`` may help track down helpful people.

    As you write your new transform or add a new kind, please consider the next
    developer.  Where possible, make your change data-driven and general, so
    that others can make a much smaller change.  Document the semantics of what
    you are changing clearly, especially if it involves modifying a transform
    schema.  And if you are adding complexity temporarily while making a
    gradual transition, please open a new bug to remind yourself to remove the
    complexity when the transition is complete.

Hacking Task Graphs
-------------------

The recommended process for changing task graphs is this:

1. Run one of the ``mach taskgraph`` subcommands (see :doc:`mach`) to
   generate a baseline against which to measure your changes.

   .. code-block:: none

       ./mach taskgraph tasks --json > old-tasks.json

2. Make your modifications under ``taskcluster/``.

3. Run the same ``mach taskgraph`` command, sending the output to a new file,
   and use ``diff`` to compare the old and new files.  Make sure your changes
   have the desired effect and no undesirable side-effects.  A plain unified
   diff should be useful for most changes, but in some cases it may be helpful
   to post-process the JSON to strip distracting changes.

4. When you are satisfied with the changes, push them to try to ensure that the
   modified tasks work as expected.

Hacking Actions
...............

If you are working on an action task and wish to test it out locally, use the
``./mach taskgraph test-action-callback`` command:

   .. code-block:: none

        ./mach taskgraph test-action-callback \
            --task-id I4gu9KDmSZWu3KHx6ba6tw --task-group-id sMO4ybV9Qb2tmcI1sDHClQ \
            --input input.yml hello_world_action

This invocation will run the hello world callback with the given inputs and
print any created tasks to stdout, rather than actually creating them.

Common Changes
--------------

Changing Test Characteristics
.............................

First, find the test description.  This will be in
``taskcluster/ci/*/tests.yml``, for the appropriate kind (consult
:doc:`kinds`).  You will find a YAML stanza for each test suite, and each
stanza defines the test's characteristics.  For example, the ``chunks``
property gives the number of chunks to run.  This can be specified as a simple
integer if all platforms have the same chunk count, or it can be keyed by test
platform.  For example:

.. code-block:: yaml

    chunks:
        by-test-platform:
            linux64/debug: 10
            default: 8

The full set of available properties is in
``taskcluster/taskgraph/transform/tests/test_description.py``.  Some other
commonly-modified properties are ``max-run-time`` (useful if tests are being
killed for exceeding maxRunTime) and ``treeherder-symbol``.

.. note::

    Android tests are also chunked at the mozharness level, so you will need to
    modify the relevant mozharness config, as well.

Adding a Test Suite
...................

To add a new test suite, you will need to know the proper mozharness invocation
for that suite, and which kind it fits into (consult :doc:`kinds`).

Add a new stanza to ``taskcluster/ci/<kind>/tests.yml``, copying from the other
stanzas in that file.  The meanings should be clear, but authoritative
documentation is in
``taskcluster/taskgraph/transform/tests/test_description.py`` should you need
it.  The stanza name is the name by which the test will be referenced in try
syntax.

Add your new test to a test set in ``test-sets.yml`` in the same directory.  If
the test should only run on a limited set of platforms, you may need to define
a new test set and reference that from the appropriate platforms in
``test-platforms.yml``.  If you do so, include some helpful comments in
``test-sets.yml`` for the next person.

Greening Up a New Test
......................

When a test is not yet reliably green, configuration for that test should not
be landed on integration branches.  Of course, you can control where the
configuration is landed!  For many cases, it is easiest to green up a test in
try: push the configuration to run the test to try along with your work to fix
the remaining test failures.

When working with a group, check out a "twig" repository to share among your
group, and land the test configuration in that repository.  Once the test is
green, merge to an integration branch and the test will begin running there as
well.

Adding a New Task
.................

If you are adding a new task that is not a test suite, there are a number of
options.  A few questions to consider:

 * Is this a new build platform or variant that will produce an artifact to
   be run through the usual test suites?

 * Does this task depend on other tasks?  Do other tasks depend on it?

 * Is this one of a few related tasks, or will you need to generate a large
   set of tasks using some programmatic means (for example, chunking)?

 * How is the task actually executed?  Mozharness?  Mach?

 * What kind of environment does the task require?

Armed with that information, you can choose among a few options for
implementing this new task.  Try to choose the simplest solution that will
satisfy your near-term needs.  Since this is all implemented in-tree, it
is not difficult to refactor later when you need more generality.

Existing Kind
`````````````

The simplest option is to add your task to an existing kind.  This is most
practical when the task "makes sense" as part of that kind -- for example, if
your task is building an installer for a new platform using mozharness scripts
similar to the existing build tasks, it makes most sense to add your task to
the ``build`` kind.  If you need some additional functionality in the kind,
it's OK to modify the implementation as necessary, as long as the modification
is complete and useful to the next developer to come along.

Tasks in the ``build`` kind generate Firefox installers, and the ``test`` kind
will add a full set of Firefox tests for each ``build`` task.

New Kind
````````

The next option to consider is adding a new kind.  A distinct kind gives you
some isolation from other task types, which can be nice if you are adding an
experimental kind of task.

Kinds can range in complexity.  The simplest sort of kind uses the transform
loader to read a list of jobs from the ``jobs`` key, and applies the standard
``job`` and ``task`` transforms:

.. code-block:: yaml

    implementation: taskgraph.task.transform:TransformTask
    transforms:
       - taskgraph.transforms.job:transforms
       - taskgraph.transforms.task:transforms
    jobs:
       - ..your job description here..

Job descriptions are defined and documented in
``taskcluster/taskgraph/transforms/job/__init__.py``.

Custom Kind Loader
``````````````````

If your task depends on other tasks, then the decision of which tasks to create
may require some code.  For example, the ``test`` kind iterates over
the builds in the graph, generating a full set of test tasks for each one.  This specific
post-build behavior is implemented as a loader defined in ``taskcluster/taskgraph/loader/test.py``.

A custom loader is useful when the set of tasks you want to create is not
static but based on something else (such as the available builds) or when the
dependency relationships for your tasks are complex.

Custom Transforms
`````````````````

Most loaders apply a series of ":doc:`transforms <transforms>`" that start with
an initial human-friendly description of a task and end with a task definition
suitable for insertion into a Taskcluster queue.

Custom transforms can be useful to apply defaults, simplifying the YAML files
in your kind. They can also apply business logic that is more easily expressed
in code than in YAML.

Transforms need not be one-to-one: a transform can produce zero or more outputs
for each input. For example, the test transforms perform chunking by producing
an output for each chunk of a given input.

Ideally those transforms will produce job descriptions, so you can use the
existing ``job`` and ``task`` transforms:

.. code-block:: yaml

    transforms:
       - taskgraph.transforms.my_stuff:transforms
       - taskgraph.transforms.job:transforms
       - taskgraph.transforms.task:transforms

Try to keep transforms simple, single-purpose and well-documented!

Custom Run-Using
````````````````

If the way your task is executed is unique (so, not a mach command or
mozharness invocation), you can add a new implementation of the job
description's "run" section.  Before you do this, consider that it might be a
better investment to modify your task to support invocation via mozharness or
mach, instead.  If this is not possible, then adding a new file in
``taskcluster/taskgraph/transforms/jobs`` with a structure similar to its peers
will make the new run-using option available for job descriptions.

Something Else?
...............

If you make another change not described here that turns out to be simple or
common, please include an update to this file in your patch.


