PL_strcpy
=========


Copies a string, up to and including the trailing ``'\0'``, into a
destination buffer.

.. _Syntax:

Syntax
~~~~~~

.. code:: eval

   char * PL_strcpy(char *dest, const char *src);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has these parameters:

``dest``
   Pointer to a buffer. On output, the buffer contains a copy of the
   string passed in src.
``src``
   Pointer to the string to be copied.

.. _Returns:

Returns
~~~~~~~

The function returns a pointer to the buffer specified by the ``dest``
parameter.

.. _Description:

Description
~~~~~~~~~~~

If the string specified by ``src`` is longer than the buffer specified
by ``dest``, the buffer will not be null-terminated.
