Auto Selector
=============

This selector automatically determines the most efficient set of tests and
tasks to run against your push. It accomplishes this via a combination of
machine learning and manual heuristics. The tasks selected here should match
pretty closely to what would be scheduled if your patch were pushed to
autoland.

It is the officially recommended selector to use when you are unsure of which
tasks should run on your push.

To use:

.. code-block:: bash

   $ mach try auto

Unlike other try selectors, tasks are not chosen locally. Rather they will be
computed by the decision task.

Like most other selectors, ``mach try auto`` supports many of the standard
templates such as ``--artifact`` or ``--env``. See ``mach try auto --help`` for
the full list of supported templates.
