Unloads a library loaded with ``PR_LoadLibrary``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prlink.h>

   PRStatus PR_UnloadLibrary(PRLibrary *lib);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has this parameter:

``lib``
   A reference previously returned from ``PR_LoadLibrary``.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Use ``PR_GetError`` to find the
   reason for the failure.

.. _Description:

Description
-----------

This function undoes the effect of a ``PR_LoadLibrary``. After calling
this function, future references to the library using its identity as
returned by ``PR_LoadLibrary`` will be invalid.
