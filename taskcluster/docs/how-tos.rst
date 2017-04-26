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

1. Find a recent decision task on the project or branch you are working on, and
   download its ``parameters.yml`` artifact.  Alternately, you
   can simply take note of the artifact URL, or just the decision task's
   ``task-id``.  This file contains all of the inputs to the task-graph
   generation process.  Its contents are simple enough if you would like to
   modify it, and it is documented in :doc:`parameters`.

2. Run one of the ``mach taskgraph`` subcommands (see :doc:`taskgraph`) to
   generate a baseline against which to measure your changes, passing the
   parameters you found in the previous step.  For example:

   .. code-block:: none

       ./mach taskgraph tasks --json -p parameters.yml > old-tasks.json
       ./mach taskgraph tasks --json -p url/to/parameters.yml > old-tasks.json
       ./mach taskgraph tasks --json -p task-id=<task-id> > old-tasks.json

3. Make your modifications under ``taskcluster/``.

4. Run the same ``mach taskgraph`` command, sending the output to a new file,
   and use ``diff`` to compare the old and new files.  Make sure your changes
   have the desired effect and no undesirable side-effects.

5. When you are satisfied with the changes, push them to try to ensure that the
   modified tasks work as expected.

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

 * How is the task actually excuted?  Mozharness?  Mach?

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

New Kind
````````

The next option to consider is adding a new kind.  A distinct kind gives you
some isolation from other task types, which can be nice if you are adding an
experimental kind of task.

Kinds can range in complexity.  The simplest sort of kind uses the
``TransformTask`` implementation to read a list of jobs from the ``jobs`` key,
and applies the standard ``job`` and ``task`` transforms:

.. code-block:: yaml

    implementation: taskgraph.task.transform:TransformTask
    transforms:
       - taskgraph.transforms.job:transforms
       - taskgraph.transforms.task:transforms
    jobs:
       - ..your job description here..

Custom Kind Implementation
``````````````````````````

If your task depends on other tasks, then the decision of which tasks to create
may require some code.  For example, the ``upload-symbols`` kind iterates over
the builds in the graph, generating a task for each one.  This specific
post-build behavior is implemented in the general
``taskgraph.task.post_build:PostBuildTask`` kind implementation.  If your task
needs something more purpose-specific, then it may be time to write a new kind
implementation.

Custom Transforms
`````````````````

If your task needs to create many tasks from a single description, for example
to implement chunking, it is time to implement some custom transforms.  Ideally
those transforms will produce job descriptions, so you can use the existing ``job``
and ``task`` transforms:

.. code-block:: yaml

    transforms:
       - taskgraph.transforms.my_stuff:transforms
       - taskgraph.transforms.job:transforms
       - taskgraph.transforms.task:transforms

Similarly, if you need to include dynamic task defaults -- perhaps some feature
is only available in level-3 repositories, or on specific projects -- then
custom transforms are the appropriate tool.  Try to keep transforms simple,
single-purpose and well-documented!

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
