Unmaps a shared memory segment identified by name.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prshm.h>

   NSPR_API( PRStatus )
     PR_DetachSharedMemory(
        PRSharedMemory *shm,
        void  *addr
   );

.. _Parameters:

Parameters
~~~~~~~~~~

The function has these parameters:

shm
   The handle returned from ``PR_OpenSharedMemory``.
addr
   The address to which the shared memory segment is mapped.

.. _Returns:

Returns
~~~~~~~

``PRStatus``.
