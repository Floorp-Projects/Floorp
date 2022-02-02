Destroys a specified lock object.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlock.h>

   void PR_DestroyLock(PRLock *lock);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_DestroyLock`` has one parameter:

``lock``
   A pointer to a lock object.

.. _Caution:

Caution
-------

The caller must ensure that no thread is currently in a lock-specific
function. Locks do not provide self-referential protection against
deletion.
