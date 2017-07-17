Actions
=======

This section shows how to define an action in-tree such that it shows up in
supported user interfaces like Treeherder.

At a very high level, the process looks like this:

 * The decision task produces an artifact, ``public/actions.json``, indicating
   what actions are available.

 * A user interface (for example, Treeherder or the Taskcluster tools) consults
   ``actions.json`` and presents appropriate choices to the user, if necessary
   gathering additional data from the user, such as the number of times to
   re-trigger a test case.

 * The user interface follows the action description to carry out the action.
   In most cases (``action.kind == 'task'``), that entails creating an "action
   task", including the provided information. That action task is responsible
   for carrying out the named action, and may create new sub-tasks if necessary
   (for example, to re-trigger a task).


.. toctree::

    action-spec
    action-uis
    action-implementation
