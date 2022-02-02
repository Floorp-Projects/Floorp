Creates or opens a named semaphore with the specified name

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prshma.h>

   NSPR_API( PRFileMap *)
   PR_OpenAnonFileMap(
     const char *dirName,
     PRSize size,
     PRFileMapProtect prot
   );

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``dirName``
   A pointer to a directory name that will contain the anonymous file.
``size``
   The size of the shared memory.
``prot``
   How the shared memory is mapped.

.. _Returns:

Returns
~~~~~~~

Pointer to ``PRFileMap`` or ``NULL`` on error.

.. _Description:

Description
-----------

If the shared memory already exists, a handle is returned to that shared
memory object.

On Unix platforms, ``PR_OpenAnonFileMap`` uses ``dirName`` as a
directory name, without the trailing '/', to contain the anonymous file.
A filename is generated for the name.

On Windows platforms, ``dirName`` is ignored.
