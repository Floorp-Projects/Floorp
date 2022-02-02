Decrement the entry count associated with a cached monitor.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prcmon.h>

   PRStatus PR_CExitMonitor(void *address);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``address``
   The address of the protected object--the same address previously
   passed to ``PR_CEnterMonitor``.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. This may indicate that the address
   parameter is invalid or that the calling thread is not in the
   monitor.

.. _Description:

Description
-----------

Using the value specified in the address parameter to find a monitor in
the monitor cache, ``PR_CExitMonitor`` decrements the entry count
associated with the monitor. If the decremented entry count is zero, the
monitor is exited.
