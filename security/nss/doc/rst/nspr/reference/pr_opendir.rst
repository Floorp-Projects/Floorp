Opens the directory with the specified pathname.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRDir* PR_OpenDir(const char *name);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``name``
   The pathname of the directory to be opened.

.. _Returns:

Returns
~~~~~~~

-  If the directory is successfully opened, a ``PRDir`` object is
   dynamically allocated and the function returns a pointer to it.
-  If the directory cannot be opened, the function returns ``NULL``.

.. _Description:

Description
-----------

``PR_OpenDir`` opens the directory specified by the pathname ``name``
and returns a pointer to a directory stream (a ``PRDir`` object) that
can be passed to subsequent ``PR_ReadDir`` calls to get the directory
entries (files and subdirectories) in the directory. The ``PRDir``
pointer should eventually be closed by a call to ``PR_CloseDir``.
