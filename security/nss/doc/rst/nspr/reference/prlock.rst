A mutual exclusion lock.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlock.h>

   typedef struct PRLock PRLock;

.. _Description:

Description
-----------

NSPR represents a lock as an opaque entity to clients of the functions
described in `"Locks" <en/NSPR_API_Reference/Locks>`__. Functions that
operate on locks do not have timeouts and are not interruptible.
