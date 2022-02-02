Allocates zeroed memory from the heap for a number of objects of a given
size.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmem.h>

   void *PR_Calloc (
      PRUint32 nelem,
      PRUint32 elsize);

.. _Parameters:

Parameters
~~~~~~~~~~

``nelem``
   The number of elements of size ``elsize`` to be allocated.
``elsize``
   The size of an individual element.

.. _Returns:

Returns
~~~~~~~

An untyped pointer to the allocated memory, or if the allocation attempt
fails, ``NULL``. Call ``PR_GetError()`` to retrieve the error returned
by the libc function ``malloc()``.

.. _Description:

Description
-----------

This function allocates memory on the heap for the specified number of
objects of the specified size. All bytes in the allocated memory are
cleared.
