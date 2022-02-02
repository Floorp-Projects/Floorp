PR_Unlock
=========

Releases a specified lock object. Releasing an unlocked lock results in
an error.

Attempting to release a lock that was locked by a different thread
causes undefined behavior.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlock.h>

   PRStatus PR_Unlock(PRLock *lock);

.. _Parameter:

Parameter
~~~~~~~~~

:ref:`PR_Unlock` has one parameter:

``lock``
   A pointer to a lock object to be released.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful (for example, if the caller does not own the lock),
   ``PR_FAILURE``.

.. _See_Also:

See Also
--------

-  `PR_Lock <PR_Lock>`__
