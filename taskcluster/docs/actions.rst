Actions
=======

This section shows how to define an action in-tree such that it shows up in
supported user interfaces like Treeherder.

At a very high level, the process looks like this:

 * The decision task produces an artifact indicating what actions are
   available.

 * The user interface consults that artifact and presents appropriate choices
   to the user, possibly involving gathering additional data from the user,
   such as the number of times to re-trigger a test case.

 * The user interface follows the action description to carry out the action.
   In most cases (``action.kind == 'task'``), that entails creating an "action
   task", including the provided information. That action task is responsible
   for carrying out the named action, and may create new sub-tasks if necessary
   (for example, to re-trigger a task).

For details on interface between in-tree logic and external user interfaces,
see :doc:`the specification for actions.json <action-spec>`.

.. toctree::

    action-details
    action-spec
