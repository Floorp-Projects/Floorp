Closes a file mapping.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_CloseFileMap(PRFileMap *fmap);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``fmap``
   The file mapping to be closed.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If the memory region is successfully unmapped, ``PR_SUCCESS``.
-  If the memory region is not successfully unmapped, ``PR_FAILURE``.
   The error code can be retrieved via ``PR_GetError``.

.. _Description:

Description
-----------

When a file mapping created with a call to ``PR_CreateFileMap`` is no
longer needed, it should be closed with a call to ``PR_CloseFileMap``.
