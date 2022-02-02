Sets the priority of a specified thread.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prthread.h>

   void PR_SetThreadPriority(
      PRThread *thread,
      PRThreadPriority priority);

.. _Parameters:

Parameters
~~~~~~~~~~

``PR_SetThreadPriority`` has the following parameters:

``thread``
   A valid identifier for the thread whose priority you want to set.
``priority``
   The priority you want to set.

.. _Description:

Description
-----------

Modifying the priority of a thread other than the calling thread is
risky. It is difficult to ensure that the state of the target thread
permits a priority adjustment without ill effects. It is preferable for
a thread to specify itself in the thread parameter when it calls
``PR_SetThreadPriority``.
