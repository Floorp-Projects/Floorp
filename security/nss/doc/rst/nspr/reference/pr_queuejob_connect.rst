Causes a job to be queued when a socket can be connected.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRJob *)
   PR_QueueJob_Connect(
     PRThreadPool *tpool,
     PRJobIoDesc *iod,
     const PRNetAddr *addr,
     PRJobFn fn,
     void * arg,
     PRBool joinable
   );

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``tpool``
   A pointer to a ``PRThreadPool`` structure previously created by a
   call to ``PR_CreateThreadPool``.
``iod``
   A pointer to a ``PRJobIoDesc`` structure.
``addr``
   Pointer to a ``PRNetAddr`` structure for the socket being connected.
``fn``
   The function to be executed when the job is executed.
``arg``
   A pointer to an argument passed to ``fn``.
``joinable``
   If ``PR_TRUE``, the job is joinable. If ``PR_FALSE``, the job is not
   joinable. See ``PR_JoinJob``.

.. _Returns:

Returns
~~~~~~~

Pointer to a ``PRJob`` structure or ``NULL`` on error.
