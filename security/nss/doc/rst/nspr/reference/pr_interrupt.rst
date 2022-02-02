Sets the interrupt request for a target thread.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prthread.h>

   PRStatus PR_Interrupt(PRThread *thread);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_Interrupt`` has the following parameter:

``thread``
   The thread whose interrupt request you want to set.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If the specified thread is currently blocked, ``PR_SUCCESS``.
-  Otherwise, ``PR_FAILURE``.

.. _Description:

Description
-----------

The purpose of ``PR_Interrupt`` is to request that a thread performing
some task stop what it is doing and return to some control point. It is
assumed that a control point has been mutually arranged between the
thread doing the interrupting and the thread being interrupted. When the
interrupted thread reaches the prearranged point, it can communicate
with its peer to discover the real reason behind the change in plans.

The interrupt request remains in the thread's state until it is
delivered exactly once or explicitly canceled. The interrupted thread
returns ``PR_FAILURE`` (-1) with an error code (see ``PR_GetError``) for
blocking operations that return a ``PRStatus`` (such as I/O operations,
monitor waits, or waiting on a condition). To check whether the thread
was interrupted, compare the result of ``PR_GetError`` with
``PR_PENDING_INTERRUPT_ERROR``.

``PR_Interrupt`` may itself fail if the target thread is invalid.

.. _Bugs:

Bugs
----

``PR_Interrupt`` has the following limitations and known bugs:

-  There can be a delay for a thread to be interrupted from a blocking
   I/O function. In all NSPR implementations, the maximum delay is at
   most five seconds. In the pthreads-based implementation on Unix, the
   maximum delay is 0.1 seconds.
-  File I/O is considered instantaneous, so file I/O functions cannot be
   interrupted. Unfortunately the standard input, output, and error
   streams are treated as files by NSPR, so a ``PR_Read`` call on
   ``PR_STDIN`` cannot be interrupted even though it may block
   indefinitely.
-  In the NT implementation, ``PR_Connect`` cannot be interrupted.
-  In the NT implementation, a file descriptor is not usable and must be
   closed after an I/O function on the file descriptor is interrupted.
   See the memo `Using IO Timeout and Interrupt on
   NT <http://www.mozilla.org/projects/nspr/tech-notes/ntiotimeoutinterrupt.html>`__
   for details.
