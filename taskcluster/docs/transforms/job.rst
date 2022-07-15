Job Transforms
==============

.. note::

   These transforms are currently duplicated by standalone Taskgraph
   and will likely be refactored / removed at a later date.

A job description says what to run in the task.  It is a combination of a
``run`` section and all of the fields from a task description.  The run section
has a ``using`` property that defines how this task should be run; for example,
``mozharness`` to run a mozharness script, or ``mach`` to run a mach command.
The remainder of the run section is specific to the run-using implementation.

The effect of a job description is to say "run this thing on this worker".  The
job description must contain enough information about the worker to identify
the workerType and the implementation (docker-worker, generic-worker, etc.).
Alternatively, job descriptions can specify the ``platforms`` field in
conjunction with the  ``by-platform`` key to specify multiple workerTypes and
implementations. Any other task-description information is passed along
verbatim, although it is augmented by the run-using implementation.

The run-using implementations are all located in
``taskcluster/gecko_taskgraph/transforms/job``, along with the schemas for their
implementations.  Those well-commented source files are the canonical
documentation for what constitutes a job description, and should be considered
part of the documentation.

following ``run-using`` are available

  * ``hazard``
  * ``mach``
  * ``mozharness``
  * ``mozharness-test``
  * ``run-task``
  * ``spidermonkey`` or ``spidermonkey-package``
  * ``debian-package``
  * ``ubuntu-package``
  * ``toolchain-script``
  * ``always-optimized``
  * ``fetch-url``
  * ``python-test``
