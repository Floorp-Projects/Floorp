Destroys a condition variable.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prcvar.h>

   void PR_DestroyCondVar(PRCondVar *cvar);

.. _Parameter:

Parameter
~~~~~~~~~

``PR_DestroyCondVar`` has one parameter:

``cvar``
   A pointer to the condition variable object to be destroyed.

.. _Description:

Description
-----------

Before calling ``PR_DestroyCondVar``, the caller is responsible for
ensuring that the condition variable is no longer in use.
