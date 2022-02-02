Retrieves the current default library path.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlink.h>

   char* PR_GetLibraryPath(void);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has no parameters.

.. _Returns:

Returns
~~~~~~~

A copy of the default library pathname string. In case of error, returns
NULL.

.. _Description:

Description
-----------

This function retrieves the current default library pathname, copies it,
and returns the copy. If sufficient storage cannot be allocated to
contain the copy, the function returns ``NULL``. Storage for the result
is allocated by the runtime and becomes the responsibility of the
caller. When it is no longer used, free it using ``PR_FreeLibraryName``.
