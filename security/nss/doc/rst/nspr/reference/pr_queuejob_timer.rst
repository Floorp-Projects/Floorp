Causes a job to be queued when a timer expires.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRJob *)
   PR_QueueJob_Timer(
     PRThreadPool *tpool,
     PRIntervalTime timeout,
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
``timeout``
   A value, expressed as a ``PRIntervalTime``, to wait before queuing
   the job.
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
