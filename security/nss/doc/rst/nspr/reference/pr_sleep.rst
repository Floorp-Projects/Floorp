Causes the current thread to yield for a specified amount of time.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prthread.h>

   PRStatus PR_Sleep(PRIntervalTime ticks);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_Sleep`` has the following parameter:

``ticks``
   The number of ticks you want the thread to sleep for (see
   ``PRIntervalTime``).

.. _Returns:

Returns
~~~~~~~

Calling ``PR_Sleep`` with a parameter equivalent to
``PR_INTERVAL_NO_TIMEOUT`` is an error and results in a ``PR_FAILURE``
error.

.. _Description:

Description
-----------

``PR_Sleep`` simply waits on a condition for the amount of time
specified. If you set ticks to ``PR_INTERVAL_NO_WAIT``, the thread
yields.

If ticks is not ``PR_INTERVAL_NO_WAIT``, ``PR_Sleep`` uses an existing
lock, but has to create a new condition for this purpose. If you have
already created such structures, it is more efficient to use them
directly.

Calling ``PR_Sleep`` with the value of ticks set to
``PR_INTERVAL_NO_WAIT`` simply surrenders the processor to ready threads
of the same priority. All other values of ticks cause ``PR_Sleep`` to
block the calling thread for the specified interval.

Threads blocked in ``PR_Sleep`` are interruptible.
