Decrements the entry count associated with a specified monitor and, if
the entry count reaches zero, releases the monitor's lock.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmon.h>

   PRStatus PR_ExitMonitor(PRMonitor *mon);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type ``PRMonitor``. The
   monitor object referenced must be one for which the calling thread
   currently holds the lock.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful (the calling thread has not entered the monitor),
   ``PR_FAILURE``.

.. _Description:

Description
-----------

If the decremented entry count is zero, ``PR_ExitMonitor`` releases the
monitor's lock. Threads that were blocked trying to enter the monitor
will be rescheduled.
