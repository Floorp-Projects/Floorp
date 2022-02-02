Constructs a full library path name.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlink.h>

   char* PR_GetLibraryName (
      const char *dir,
      const char *lib);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has these parameters:

``dir``
   A ``NULL``-terminated string representing the path name of the
   library, as returned by ``PR_GetLibraryPath``.
``lib``
   The leaf name of the library of interest.

.. _Returns:

Returns
~~~~~~~

If successful, returns a new character string containing a constructed
path name. In case of error, returns ``NULL``.

.. _Description:

Description
-----------

This function constructs a full path name from the specified directory
name and library name. The constructed path name refers to the actual
dynamically loaded library. It is suitable for use in the
``PR_LoadLibrary`` call.

This function does not test for existence of the specified file, it just
constructs the full filename. The way the name is constructed is system
dependent.

If sufficient storage cannot be allocated to contain the constructed
path name, the function returns ``NULL``. Storage for the result is
allocated by the runtime and becomes the responsibility of the caller.
When it is no longer used, free it using ``PR_FreeLibraryName``.
