Creates a string identifying a ``PRFileMap``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prshma.h>

   NSPR_API( PRStatus )
   PR_ExportFileMapAsString(
     PRFileMap *fm,
     PRSize bufsize,
     char *buf
   );

#. define PR_FILEMAP_STRING_BUFSIZE 128

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fm``
   A pointer to the ``PRFileMap`` to be represented as a string.
``bufsize``
   sizeof(buf)
``buf``
   A pointer to abuffer of length ``PR_FILEMAP_STRING_BUFSIZE``.

.. _Returns:

Returns
~~~~~~~

``PRStatus``

.. _Description:

Description
-----------

Creates an identifier, as a string, from a ``PRFileMap`` object
previously created with ``PR_OpenAnonFileMap``.
