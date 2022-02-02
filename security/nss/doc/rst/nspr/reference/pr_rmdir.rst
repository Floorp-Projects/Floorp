PR_RmDir
========

Removes a directory with a specified name.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_RmDir(const char *name);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``name``
   The name of the directory to be removed.

.. _Returns:

Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The actual reason can be retrieved
   via :ref:`PR_GetError`.

.. _Description:

Description
-----------

:ref:`PR_RmDir` removes the directory specified by the pathname ``name``.
The directory must be empty. If the directory is not empty, :ref:`PR_RmDir`
fails and :ref:`PR_GetError` returns the error code
``PR_DIRECTORY_NOT_EMPTY_ERROR``.

.. _See_Also:

See Also
--------

:ref:`PR_MkDir`
