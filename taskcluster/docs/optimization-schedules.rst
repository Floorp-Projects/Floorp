Optimization and SCHEDULES
==========================

Most optimization of builds and tests is handled with ``SCHEDULES``.
The concept is this: we allocate tasks into named components, and associate a set of such components to each file in the source tree.
Given a set of files changed in a push, we then calculate the union of components affected by each file, and remove tasks that are not tagged with any of them.

This optimization system is intended to be *conservative*.
It represents what could *possibly* be affected, rather than any intuitive notion of what tasks would be useful to run for changes to a particular file.
For example:

* ``dom/url/URL.cpp`` schedules tasks on all platform and could potentially cause failures in any test suite

* ``dom/system/mac/CoreLocationLocationProvider.mm`` could not possibly affect any platform but ``macosx``, but potentially any test suite

* ``python/mozbuild/mozbuild/preprocessor.py`` could possibly affect any platform, and should also schedule Python lint tasks

Exclusive and Inclusive
-----------------------

The first wrinkle in this "simple" plan is that there are a lot of files, and for the most part they all affect most components.
But there are some components which are only affected by a well-defined set of files.
For example, a Python lint component need only be scheduled when Python files are changed.

We divide the components into "exclusive" and "inclusive" components.
Absent any other configuration, any file in the repository is assumed to affect all of the exclusive components and none of the inclusive components.

Exclusive components can be thought of as a series of families.
For example, the platform (linux, windows, macosx, android) is a component family.
The test suite (mochitest, reftest, xpcshell, etc.) is another.
By default, source files are associated with every component in every family.
This means tasks tagged with an exclusive component will *always* run, unless none of the modified source files are associated with that component.

But what if we only want to run a particular task when a pre-determined file is modified?
This is where inclusive components are used.
Any task tagged with an inclusive component will *only* be run when a source file associated with that component is modified.
Lint tasks and well separated unittest tasks are good examples of things you might want to schedule inclusively.

A good way to keep this straight is to think of exclusive platform-family components (``macosx``, ``android``, ``windows``, ``linux``) and inclusive linting components (``py-lint``, ``js-lint``).
An arbitrary file in the repository affects all platform families, but does not necessarily require a lint run.
But we can configure mac-only files such as ``CoreLocationLocationProvider.mm`` to affect exclusively ``macosx``, and Python files like ``preprocessor.py`` to affect ``py-lint`` in addition to the exclusive components.

It is also possible to define a file as affecting an inclusive component and nothing else.
For example, the source code and configuration for the Python linting tasks does not affect any tasks other than linting.

.. note:

    Most unit test suite tasks are allocated to components for their platform family and for the test suite.
    This indicates that if a platform family is affected (for example, ``android``) then the builds for that platform should execute as well as the full test suite.
    If only a single suite is affected (for example, by a change to a reftest source file), then the reftests should execute for all platforms.

    However, some test suites, for which the set of contributing files are well-defined, are represented as inclusive components.
    These components will not be executed by default for any platform families, but only when one or more of the contributing files are changed.

Specification
-------------

Components are defined as either inclusive or exclusive in :py:mod:`mozbuild.schedules`.

File Annotation
:::::::::::::::

Files are annotated with their affected components in ``moz.build`` files with stanzas like ::

    
    with Files('**/*.py'):
        SCHEDULES.inclusive += ['py-lint']

for inclusive components and ::

    with Files('*gradle*'):
        SCHEDULES.exclusive = ['android']

for exclusive components.
Note the use of ``+=`` for inclusive compoenents (as this is adding to the existing set of affected components) but ``=`` for exclusive components (as this is resetting the affected set to something smaller).
For cases where an inclusive component is affected exclusively (such as the python-lint configuration in the example above), that component can be assigned to ``SCHEDULES.exclusive``::

    with Files('**/pep8rc'):
        SCHEDULES.exclusive = ['py-lint']

Task Annotation
:::::::::::::::

Tasks are annotated with the components they belong to using the ``"skip-unless-schedules"`` optimization, which takes a list of components for this task::

    task['optimization'] = {'skip-unless-schedules': ['windows', 'gtest']}

For tests, this value is set automatically by the test transform based on the suite name and the platform family, doing the correct thing for inclusive test suites.
Tests also use SETA via ``"skip-unless-schedules-or-seta"``, which skips a task if it is not affected *or* if SETA deems it unimportant.
