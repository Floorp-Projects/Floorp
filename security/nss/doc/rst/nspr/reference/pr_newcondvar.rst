Creates a new condition variable.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prcvar.h>

   PRCondVar* PR_NewCondVar(PRLock *lock);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_NewCondVar`` has one parameter:

``lock``
   The identity of the mutex that protects the monitored data, including
   this condition variable.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to the new condition variable object.
-  If unsuccessful (for example, if system resources are unavailable),
   ``NULL``.
