Try
===

"Try" is a way to "try out" a proposed change safely before review, without
officialy landing it.  This functionality has been around for a *long* time in
various forms, and can sometimes show its age.

Access to "push to try" is typically avilable to a much larger group of
developers than those who can land changes in integration and release branches.
Specifically, try pushes are allowed for anyone with `SCM Level`_ 1, while
integration branches are at SCM level 3.

Scheduling a Task on Try
------------------------

There are two methods for scheduling a task on try.

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
of tasks is usually generated locally with some `local tool <tryselect>`_ and
attached to the commit pushed to the try repository. This gives finer control
over exactly what runs and enables growth of an ecosystem of tooling
appropriate to varied circumstances.

Implementation
,,,,,,,,,,,,,,

This method uses a checked-in file called ``try_task_config.json`` which lives
at the root of the source dir. The JSON object in this file contains a
``tasks`` key giving the labels of the tasks to run.  For example, the
``try_task_config.json`` file might look like:

.. parsed-literal::

    {
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
try, it is mainly meant to be a generation target for various `tryselect`_
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

It's possible to alter the definition of a task with templates. Templates are
`JSON-e`_ files that live in the `taskgraph module`_. Templates can be specified
from the ``try_task_config.json`` like this:

.. parsed-literal::

    {
      "tasks": [...],
      "templates": {
        artifact: {"enabled": 1}
      }
    }

Each key in the templates object denotes a new template to apply, and the value
denotes extra context to use while rendering. When specified, a template will
be applied to every task no matter what. If the template should only be applied
to certain kinds of tasks, this needs to be specified in the template itself
using JSON-e `condition statements`_.

The context available to the JSON-e render aims to match that of ``actions``.
It looks like this:

.. parsed-literal::

    {
      "task": {
        "payload": {
          "env": { ... },
          ...
        }
        "extra": {
          "treeherder": { ... },
          ...
        },
        "tags": { "kind": "<kind>", ... },
        ...
      },
      "input": {
        "enabled": 1,
        ...
      },
      "taskId": "<task id>"
    }

See the `existing templates`_ for examples.

.. _tryselect: https://dxr.mozilla.org/mozilla-central/source/tools/tryselect
.. _JSON-e: https://taskcluster.github.io/json-e/
.. _taskgraph module: https://dxr.mozilla.org/mozilla-central/source/taskcluster/taskgraph/templates
.. _condition statements: https://taskcluster.github.io/json-e/#%60$if%60%20-%20%60then%60%20-%20%60else%60
.. _existing templates: https://dxr.mozilla.org/mozilla-central/source/taskcluster/taskgraph/templates
.. _SCM Level: https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/
