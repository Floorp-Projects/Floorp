Allocates memory of a specified size from the heap.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmem.h>

   _type * PR_NEW(_struct);

.. _Parameter:

Parameter
~~~~~~~~~

``_struct``
   The name of a type.

.. _Returns:

Returns
~~~~~~~

An pointer to a buffer sized to contain the type ``_struct``, or if the
allocation attempt fails, ``NULL``. Call ``PR_GetError()`` to retrieve
the error returned by the libc function.

.. _Description:

Description
-----------

This macro allocates memory whose size is ``sizeof(_struct)`` and
returns a pointer to that memory.
