PR_Delete
=========

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_Delete(const char *name);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameter:

``name``
   The pathname of the file to be deleted.

.. _Returns:

Returns
~~~~~~~

One of the following values:

-  If file is deleted successfully, ``PR_SUCCESS``.
-  If the file is not deleted, ``PR_FAILURE``.

.. _Description:

Description
-----------

:ref:`PR_Delete` deletes a file with the specified pathname ``name``. If
the function fails, the error code can then be retrieved via
:ref:`PR_GetError`.

 
