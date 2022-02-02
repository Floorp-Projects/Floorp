Returns the current time.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtime.h>

   PRTime PR_Now(void);

.. _Parameters:

Parameters
~~~~~~~~~~

None.

.. _Returns:

Returns
~~~~~~~

The current time as a ``PRTime`` value.

.. _Description:

Description
-----------

``PR_Now()`` returns the current time as number of microseconds since
the NSPR epoch, which is midnight (00:00:00) 1 January 1970 UTC.

You cannot assume that the values returned by ``PR_Now()`` are
monotonically increasing because the system clock of the computer may be
reset. To obtain monotonically increasing time stamps suitable for
measuring elapsed time, use ``PR_IntervalNow()``.
