Frees allocated memory in the heap.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmem.h>

   void PR_Free(void *ptr);

.. _Parameter:

Parameter
~~~~~~~~~

``ptr``
   A pointer to the memory to be freed.

.. _Returns:

Returns
~~~~~~~

Nothing.

.. _Description:

Description
-----------

This function frees the memory addressed by ``ptr`` in the heap.
