Closes a file descriptor.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_Close(PRFileDesc *fd);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a ``PRFileDesc`` object.

.. _Returns:

Returns
~~~~~~~

One of the following values:

-  If file descriptor is closed successfully, ``PR_SUCCESS``.
-  If the file descriptor is not closed successfully, ``PR_FAILURE``.

.. _Description:

Description
-----------

The file descriptor may represent a normal file, a socket, or an end
point of a pipe. On successful return, ``PR_Close`` frees the dynamic
memory and other resources identified by the ``fd`` parameter.
