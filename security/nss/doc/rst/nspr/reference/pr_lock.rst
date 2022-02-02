Locks a specified lock object.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlock.h>

   void PR_Lock(PRLock *lock);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_Lock`` has one parameter:

``lock``
   A pointer to a lock object to be locked.

.. _Description:

Description
-----------

When ``PR_Lock`` returns, the calling thread is "in the monitor," also
called "holding the monitor's lock." Any thread that attempts to acquire
the same lock blocks until the holder of the lock exits the monitor.
Acquiring the lock is not an interruptible operation, nor is there any
timeout mechanism.

``PR_Lock`` is not reentrant. Calling it twice on the same thread
results in undefined behavior.

.. _See_Also:

See Also
--------

-  ``PR_Unlock``
