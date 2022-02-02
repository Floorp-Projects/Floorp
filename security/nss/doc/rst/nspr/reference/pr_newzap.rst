Allocates and clears memory from the heap for an instance of a given
type.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmem.h>

   _type * PR_NEWZAP(_struct);

.. _Parameter:

Parameter
~~~~~~~~~

``_struct``
   The name of a type.

.. _Returns:

Returns
~~~~~~~

An pointer to a buffer sized to contain the type ``_struct``, or if the
allocation attempt fails, ``NULL``. The bytes in the buffer are all
initialized to 0. Call ``PR_GetError()`` to retrieve the error returned
by the libc function.

.. _Description:

Description
-----------

This macro allocates an instance of the specified type from the heap and
sets the content of that memory to zero.
