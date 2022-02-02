Renames a file.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_Rename(
     const char *from,
     const char *to);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``from``
   The old name of the file to be renamed.
``to``
   The new name of the file.

.. _Returns:

Returns
~~~~~~~

One of the following values:

-  If file is successfully renamed, ``PR_SUCCESS``.
-  If file is not successfully renamed, ``PR_FAILURE``.

.. _Description:

Description
-----------

``PR_Rename`` renames a file from its old name (``from``) to a new name
(``to``). If a file with the new name already exists, ``PR_Rename``
fails with the error code ``PR_FILE_EXISTS_ERROR``. In this case,
``PR_Rename`` does not overwrite the existing filename.
