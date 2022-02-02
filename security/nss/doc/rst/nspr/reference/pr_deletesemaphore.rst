Removes a semaphore specified by name from the system.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <pripcsem.h>

   NSPR_API(PRStatus) PR_DeleteSemaphore(const char *name);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``name``
   The name of a semaphore that was previously created via a call to
   ``PR_OpenSemaphore``.

.. _Returns:

Returns
~~~~~~~

``PRStatus``
