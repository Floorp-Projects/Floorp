Waits for all threads in a thread pool to complete, then releases
resources allocated to the thread pool.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRStatus) PR_JoinThreadPool( PRThreadPool *tpool );

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``tpool``
   A pointer to a ``PRThreadPool`` structure previously created by a
   call to ``PR_CreateThreadPool``.

.. _Returns:

Returns
~~~~~~~

``PRStatus``
