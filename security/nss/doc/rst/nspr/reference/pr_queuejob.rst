Queues a job to a thread pool for execution.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRJob *)
   PR_QueueJob(
     PRThreadPool *tpool,
     PRJobFn fn,
     void *arg,
     PRBool joinable
   );

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``tpool``
   A pointer to a ``PRThreadPool`` structure previously created by a
   call to ``PR_CreateThreadPool``.
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
