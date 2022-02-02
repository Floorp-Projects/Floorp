This chapter describes the NSPR API Thread Pools.

.. note::

   **Note:** This API is a preliminary version in NSPR 4.0 and is
   subject to change.

Thread pools create and manage threads to provide support for scheduling
work (jobs) onto one or more threads. NSPR's thread pool is modeled on
the thread pools described by David R. Butenhof in\ *Programming with
POSIX Threads* (Addison-Wesley, 1997).

-  `Thread Pool Types <#Thread_Pool_Types>`__
-  `Thread Pool Functions <#Thread_Pool_Functions>`__

.. _Thread_Pool_Types:

Thread Pool Types
-----------------

-  ``PRJobIoDesc``
-  ``PRJobFn``
-  ``PRThreadPool``
-  ``PRJob``

.. _Thread_Pool_Functions:

Thread Pool Functions
---------------------

-  ``PR_CreateThreadPool``
-  ``PR_QueueJob``
-  ``PR_QueueJob_Read``
-  ``PR_QueueJob_Write``
-  ``PR_QueueJob_Accept``
-  ``PR_QueueJob_Connect``
-  ``PR_QueueJob_Timer``
-  ``PR_CancelJob``
-  ``PR_JoinJob``
-  ``PR_ShutdownThreadPool``
-  ``PR_JoinThreadPool``
