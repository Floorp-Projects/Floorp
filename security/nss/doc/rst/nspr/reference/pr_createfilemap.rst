Creates a file mapping object.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRFileMap* PR_CreateFileMap(
     PRFileDesc *fd,
     PRInt64 size,
     PRFileMapProtect prot);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a ``PRFileDesc`` object representing the file that is to
   be mapped to memory.
``size``
   Size of the file specified by ``fd``.
``prot``
   Protection option for read and write accesses of a file mapping. This
   parameter consists of one of the following options:

   -  ``PR_PROT_READONLY``. Read-only access.
   -  ``PR_PROT_READWRITE``. Readable, and write is shared.
   -  ``PR_PROT_WRITECOPY``. Readable, and write is private
      (copy-on-write).

.. _Returns:

Returns
~~~~~~~

-  If successful, a file mapping of type ``PRFileMap``.
-  If unsuccessful, ``NULL``.

.. _Description:

Description
-----------

The ``PRFileMapProtect`` enumeration used in the ``prot`` parameter is
defined as follows:

.. code:: eval

   typedef enum PRFileMapProtect {
     PR_PROT_READONLY,
     PR_PROT_READWRITE,
     PR_PROT_WRITECOPY
   } PRFileMapProtect;

``PR_CreateFileMap`` only prepares for the mapping a file to memory. The
returned file-mapping object must be passed to ``PR_MemMap`` to actually
map a section of the file to memory.

The file-mapping object should be closed with a ``PR_CloseFileMap`` call
when it is no longer needed.
