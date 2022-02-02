Imports a ``PRFileMap`` previously exported by my parent process via
``PR_CreateProcess``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prshma.h>

   NSPR_API( PRFileMap *)
   PR_GetInheritedFileMap(
     const char *shmname
   );

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``shmname``
   The name provided to ``PR_ProcessAttrSetInheritableFileMap``.

.. _Returns:

Returns
~~~~~~~

Pointer to ``PRFileMap`` or ``NULL`` on error.

.. _Description:

Description
-----------

``PR_GetInheritedFileMap`` retrieves a PRFileMap object exported from
its parent process via ``PR_CreateProcess``.

.. note::

   **Note:** This function is not implemented.
