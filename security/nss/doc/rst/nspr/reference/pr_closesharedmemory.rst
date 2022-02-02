Closes a shared memory segment identified by name.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prshm.h>

   NSPR_API( PRStatus )
     PR_CloseSharedMemory(
        PRSharedMemory *shm
   );

.. _Parameter:

Parameter
~~~~~~~~~

The function has these parameter:

shm
   The handle returned from ``PR_OpenSharedMemory``.

.. _Returns:

Returns
~~~~~~~

``PRStatus``.
