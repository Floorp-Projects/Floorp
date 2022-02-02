Creates a directory with a specified name and access mode.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_MkDir(
     const char *name,
     PRIntn mode);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``name``
   The name of the directory to be created. All the path components up
   to but not including the leaf component must already exist.
``mode``
   The access permission bits of the file mode of the new directory if
   the file is created when ``PR_CREATE_FILE`` is on.

Caveat: The mode parameter is currently applicable only on Unix
platforms. It may be applicable to other platforms in the future.

Possible values include the following:

-  ``00400``. Read by owner.
-  ``00200``. Write by owner.
-  ``00100``. Search by owner.
-  ``00040``. Read by group.
-  ``00020``. Write by group.
-  ``00010``. Search by group.
-  ``00004``. Read by others.
-  ``00002``. Write by others.
-  ``00001``. Search by others.

.. _Returns:

Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The actual reason can be retrieved
   via ``PR_GetError``.

.. _Description:

Description
-----------

``PR_MkDir`` creates a new directory with the pathname ``name``. All the
path components up to but not including the leaf component must already
exist. For example, if the pathname of the directory to be created is
``a/b/c/d``, the directory ``a/b/c`` must already exist.

.. _See_Also:

See Also
--------

``PR_RmDir``
