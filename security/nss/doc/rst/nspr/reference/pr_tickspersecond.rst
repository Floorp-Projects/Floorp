Returns the number of ticks per second currently used to determine the
value of ``PRIntervalTime``.

.. _Syntax:

Syntax
------

.. code:: eval

    #include <prinrval.h>

    PRUint32 PR_TicksPerSecond(void);

.. _Returns:

Returns
~~~~~~~

An integer between 1000 and 100000 indicating the number of ticks per
second counted by ``PRIntervalTime`` on the current platform. This value
is platform-dependent and does not change after NSPR is initialized.

.. _Description:

Description
-----------

The value returned by ``PR_TicksPerSecond()`` lies between
``PR_INTERVAL_MIN`` and ``PR_INTERVAL_MAX``.

The relationship between a ``PRIntervalTime`` tick and standard clock
units is platform-dependent. PR\_\ ``PR_TicksPerSecond()`` allows you to
discover exactly what that relationship is. Seconds per tick (the
inverse of PR\_\ ``PR_TicksPerSecond()``) is always between 10
microseconds and 1 millisecond.
