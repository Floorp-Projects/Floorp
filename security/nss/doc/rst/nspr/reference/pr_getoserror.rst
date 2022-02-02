Returns the current thread's last set OS-specific error code.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prerror.h>

   PRInt32 PR_GetOSError(void)

.. _Returns:

Returns
~~~~~~~

The value returned is a 32-bit signed number. Its interpretation is left
to the caller.

.. _Description:

Description
-----------

Used for platform-specific code that requires the underlying OS error.
For portability, clients should not create dependencies on the values of
OS-specific error codes. However, this information is preserved, along
with a platform neutral error code, on a per thread basis. It is most
useful during development.
