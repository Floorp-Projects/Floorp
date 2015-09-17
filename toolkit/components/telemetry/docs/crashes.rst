=======
Crashes
=======

There are many different kinds of crashes for Firefox, there is not a single system used to record all of them.

Main process crashes
====================

If the Firefox main process dies, that should be recorded as an aborted session. We would submit a :doc:`main ping <main-ping>` with the reason ``aborted-session``.
If we have a crash dump for that crash, we should also submit a :doc:`crash ping <crash-ping>`.

The ``aborted-session`` information is first written to disk 60 seconds after startup, any earlier crashes will not trigger an ``aborted-session`` ping.
Also, the ``aborted-session`` is updated at least every 5 minutes, so it may lag behind the last session state.

Crashes during startup should be recorded in the next sessions main ping in the ``STARTUP_CRASH_DETECTED`` histogram.

Child process crashes
=====================

If a Firefox plugin, content or gmplugin process dies unexpectedly, this is recorded in the main pings ``SUBPROCESS_ABNORMAL_ABORT`` keyed histogram.

If we catch a crash report for this, then additionally the ``SUBPROCESS_CRASHES_WITH_DUMP`` keyed histogram is incremented.
