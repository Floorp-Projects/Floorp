Determines the number of bytes (expressed as a 32-bit integer) that are
available for reading beyond the current read-write pointer in a
specified file or socket.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRInt64 PR_Available64(PRFileDesc *fd);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``fd``
   Pointer to a ``PRFileDesc`` object representing a file or socket.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If the function completes successfully, it returns the number of
   bytes that are available for reading. For a normal file, these are
   the bytes beyond the current file pointer.
-  If the function fails, it returns the value -1. The error code can
   then be retrieved via ``PR_GetError``.

.. _Description:

Description
-----------

``PR_Available64`` works on normal files and sockets. ``PR_Available``
does not work with pipes on Win32 platforms.

.. _See_Also:

See Also
--------

If the number of bytes available for reading is within the range of a
32-bit integer, use ``PR_Available``.
