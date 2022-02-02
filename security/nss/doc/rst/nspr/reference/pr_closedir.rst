Closes the specified directory.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_CloseDir(PRDir *dir);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``dir``
   A pointer to a ``PRDir`` structure representing the directory to be
   closed.

.. _Returns:

Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   retrieved via ``PR_GetError``.

.. _Description:

Description
-----------

When a ``PRDir`` object is no longer needed, it must be closed and freed
with a call to ``PR_CloseDir`` call. Note that after a ``PR_CloseDir``
call, any ``PRDirEntry`` object returned by a previous ``PR_ReadDir``
call on the same ``PRDir`` object becomes invalid.

.. _See_Also:

See Also
--------

``PR_OpenDir``
