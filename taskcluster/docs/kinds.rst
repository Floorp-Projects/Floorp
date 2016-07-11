Task Kinds
==========

This section lists and documents the available task kinds.

Builds
------

Builds are currently implemented by the ``legacy`` kind.

Tests
-----

Test tasks for Gecko products are divided into several kinds, but share a
common implementation.  The process goes like this, based on a set of YAML
files named in ``kind.yml``:

 * For each build task, determine the related test platforms based on the build
   platform.  For example, a Windows 2010 build might be tested on Windows 7
   and Windows 10.  Each test platform specifies a "test set" indicating which
   tests to run.  This is configured in the file named
   ``test-platforms.yml``.

 * Each test set is expanded to a list of tests to run.  This is configured in
   the file named by ``test-sets.yml``.

 * Each named test is looked up in the file named by ``tests.yml`` to find a
   test description.  This test description indicates what the test does, how
   it is reported to treeherder, and how to perform the test, all in a
   platform-independent fashion.

 * Each test description is converted into one or more tasks.  This is
   performed by a sequence of transforms defined in the ``transforms`` key in
   ``kind.yml``.  See :doc:`transforms`: for more information on these
   transforms.

 * The resulting tasks become a part of the task graph.

.. important::

    This process generates *all* test jobs, regardless of tree or try syntax.
    It is up to a later stage of the task-graph generation (the target set) to
    select the tests that will actually be performed.

desktop-test
............

The ``desktop-test`` kind defines tests for Desktop builds.  Its ``tests.yml``
defines the full suite of desktop tests and their particulars, leaving it to
the transforms to determine how those particulars apply to Linux, OS X, and
Windows.

android-test
............

The ``android-test`` kind defines tests for Android builds.

It is very similar to ``desktop-test``, but the details of running the tests
differ substantially, so they are defined separately.

legacy
------

The legacy kind is the old, templated-yaml-based task definition mechanism.  It
is still used for builds and generic tasks, but not for long!

docker-image
------------

Tasks of the ``docker-image`` kind build the Docker images in which other
Docker tasks run.

The tasks to generate each docker image have predictable labels:
``build-docker-image-<name>``.

Docker images are built from subdirectories of ``testing/docker``, using
``docker build``.  There is currently no capability for one Docker image to
depend on another in-tree docker image, without uploading the latter to a
Docker repository

The task definition used to create the image-building tasks is given in
``image.yml`` in the kind directory, and is interpreted as a :doc:`YAML
Template <yaml-templates>`.
