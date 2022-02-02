PR_CNotifyAll
=============

Notifies all the threads waiting for a change in the state of monitored
data.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prcmon.h>

   PRStatus PR_CNotifyAll(void *address);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``address``
   The address of the monitored object. The calling thread must be in
   the monitor at the time :ref:`PR_CNotifyAll` is called.

.. _Returns:

Returns
~~~~~~~

 - :ref:`PR_SUCCESS` indicates that the referenced monitor was located and
   the calling thread was in the monitor.
 - :ref:`PR_FAILURE` indicates that the referenced monitor could not be
   located or that the calling thread was not in the monitor

.. _Description:

Description
-----------

Using the value specified in the address parameter to find a monitor in
the monitor cache, :ref:`PR_CNotifyAll` notifies all threads waiting for
the monitor's state to change. All of the threads waiting on the state
change are then scheduled to reenter the monitor.
