PR_AtomicAdd
============

.. _Syntax:

Syntax
------

.. code:: eval

   #include <pratom.h>

   PRInt32 PR_AtomicAdd(
     PRInt32 *ptr,
     PRInt32 val);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameters:

``ptr``
   A pointer to the value to increment.
``val``
   A value to be added.

.. _Returns:

Returns
~~~~~~~

The returned value is the result of the addition.

.. _Description:

Description
-----------

Atomically add a 32 bit value.

 
