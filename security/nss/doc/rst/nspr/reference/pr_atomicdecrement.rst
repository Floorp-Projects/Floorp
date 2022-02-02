Atomically decrements a 32-bit value.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <pratom.h>

   PRInt32 PR_AtomicDecrement(PRInt32 *val);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``val``
   A pointer to the value to decrement.

.. _Returns:

Returns
~~~~~~~

The function returns the decremented value (i.e., the result).

.. _Description:

Description
-----------

``PR_AtomicDecrement`` first decrements the referenced variable by one.
The value returned is the referenced variable's final value. The
modification to memory is unconditional.
