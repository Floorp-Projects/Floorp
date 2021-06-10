Try
===

"Try" is a way to "try out" a proposed change safely before review, without
officially landing it.  This functionality has been around for a *long* time in
various forms, and can sometimes show its age.

Access to "push to try" is typically available to a much larger group of
developers than those who can land changes in integration and release branches.
Specifically, try pushes are allowed for anyone with `SCM Level`_ 1, while
integration branches are at SCM level 3.

Scheduling a Task on Try
------------------------

There are three methods for scheduling a task on try: legacy try option syntax,
try task config, and an empty try.

Try Option Syntax
:::::::::::::::::

The first, older method is a command line string called ``try syntax`` which is passed
into the decision task via the commit message. The resulting commit is then
pushed to the https://hg.mozilla.org/try repository.  An example try syntax
might look like:

.. parsed-literal::

    try: -b o -p linux64 -u mochitest-1 -t none

This gets parsed by ``taskgraph.try_option_syntax:TryOptionSyntax`` and returns
a list of matching task labels. For more information see the
`TryServer wiki page <https://wiki.mozilla.org/Try>`_.

Try Task Config
:::::::::::::::

The second, more modern method specifies exactly the tasks to run.  That list
of tasks is usually generated locally with some :doc:`local tool </tools/try/selectors/fuzzy>`
and attached to the commit pushed to the try repository. This gives
finer control over exactly what runs and enables growth of an
ecosystem of tooling appropriate to varied circumstances.

Implementation
,,,,,,,,,,,,,,

This method uses a checked-in file called ``try_task_config.json`` which lives
at the root of the source dir. The JSON object in this file contains a
``tasks`` key giving the labels of the tasks to run.  For example, the
``try_task_config.json`` file might look like:

.. parsed-literal::

    {
      "version": 1,
      "tasks": [
        "test-windows10-64/opt-web-platform-tests-12",
        "test-windows7-32/opt-reftest-1",
        "test-windows7-32/opt-reftest-2",
        "test-windows7-32/opt-reftest-3",
        "build-linux64/debug",
        "source-test-mozlint-eslint"
      ]
    }

Very simply, this will run any task label that gets passed in as well as their
dependencies. While it is possible to manually commit this file and push to
try, it is mainly meant to be a generation target for various :ref:`try server <Pushing to Try>`
choosers.  For example:

.. parsed-literal::

    $ ./mach try fuzzy

A list of all possible task labels can be obtained by running:

.. parsed-literal::

    $ ./mach taskgraph tasks

A list of task labels relevant to a tree (defaults to mozilla-central) can be
obtained with:

.. parsed-literal::

    $ ./mach taskgraph target

Modifying Tasks in a Try Push
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

It's possible to alter the definition of a task by defining additional
configuration in ``try_task_config.json``. For example, to set an environment
variable in all tasks, you can add:

.. parsed-literal::

    {
      "version": 1,
      "tasks": [...],
      "env": {
        "FOO": "bar"
      }
    }

The allowed configs are defined in :py:obj:`taskgraph.decision.try_task_config_schema`.
The values will be available to all transforms, so how each config applies will
vary wildly from one context to the next. Some (such as ``env``) will affect
all tasks in the graph. Others might only affect certain kinds of task. The
``use-artifact-builds`` config only applies to build tasks for instance.

Empty Try
:::::::::

If there is no try syntax or ``try_task_config.json``, the ``try_mode``
parameter is None and no tasks are selected to run.  The resulting push will
only have a decision task, but one with an "add jobs" action that can be used
to add the desired jobs to the try push.


Complex Configuration
:::::::::::::::::::::

If you need more control over the build configuration,
(:doc:`staging releases </tools/try/selectors/release>`, for example),
you can directly specify :doc:`parameters <parameters>`
to override from the ``try_task_config.json`` like this:

.. parsed-literal::

   {
       "version": 2,
       "parameters": {
           "optimize_target_tasks": true,
           "release_type": "beta",
           "target_tasks_method": "staging_release_builds"
       }
   }

This format can express a superset of the version 1 format, as the
version one configuration is equivalent to the following version 2
config.

.. parsed-literal::

   {
       "version": 2,
       "parameters": {
           "try_task_config": {...},
           "try_mode": "try_task_config",
       }
   }

.. _SCM Level: https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/
