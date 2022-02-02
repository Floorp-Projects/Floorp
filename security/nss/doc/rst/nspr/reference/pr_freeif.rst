PR_FREEIF
=========

Conditionally frees allocated memory.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmem.h>

   void PR_FREEIF(_ptr);

.. _Parameter:

Parameter
~~~~~~~~~

``_ptr``
   The address of memory to be returned to the heap.

.. _Returns:

Returns
~~~~~~~

Nothing.

.. _Description:

Description
-----------

This macro returns memory to the heap when ``_ptr`` is not ``NULL``. If
``_ptr`` is ``NULL``, the macro has no effect.
