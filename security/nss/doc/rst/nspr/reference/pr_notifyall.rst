Promotes all threads waiting on a specified monitor to a ready state.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmon.h>

   PRStatus PR_NotifyAll(PRMonitor *mon);

.. _Parameters:

Parameters
~~~~~~~~~~

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
-  If unsuccessful, ``PR_FAILURE``.

.. _Description:

Description
-----------

A call to ``PR_NotifyAll`` causes all of the threads waiting on the
monitor to be scheduled to be promoted to a ready state. If no threads
are waiting, the operation is no-op.

``PR_NotifyAll`` should be used with some care. The expense of
scheduling multiple threads increases dramatically as the number of
threads increases.
