PL_strdup
=========

Returns a pointer to a new memory node in the NSPR heap containing a
copy of a specified string.

.. _Syntax:

Syntax
~~~~~~

.. code:: eval

   #include <plstr.h>

   char *PL_strdup(const char *s);

.. _Parameter:

Parameter
~~~~~~~~~

The function has a single parameter:

``s``
   The string to copy, may be ``NULL``.

.. _Returns:

Returns
~~~~~~~

The function returns one of these values:

-  If successful, a pointer to a copy of the specified string.
-  If the memory allocation fails, ``NULL``.

.. _Description:

Description
~~~~~~~~~~~

To accommodate the terminator, the size of the allocated memory is one
greater than the length of the string being copied. A ``NULL`` argument,
like a zero-length argument, results in a pointer to a one-byte block of
memory containing the null value.

.. _Notes:

Notes
~~~~~

The memory allocated by :ref:`PL_strdup` should be freed with
`PL_strfree </en/PL_strfree>`__.
