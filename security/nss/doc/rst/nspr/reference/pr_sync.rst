Synchronizes any buffered data for a file descriptor to its backing
device (disk).

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_Sync(PRFileDesc *fd);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``fd``
   Pointer to a ``PRFileDesc`` object representing a file.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  On successful completion, ``PR_SUCCESS``.
-  If the function fails, ``PR_FAILURE``.

.. _Description:

Description
-----------

``PR_Sync`` writes all the in-memory buffered data of the specified file
to the disk.
