Creates a ``PRFileMap`` from an identifying string.

.. _Syntax:

Syntax
~~~~~~

.. code:: eval

   #include <prshma.h>

   NSPR_API( PRFileMap * )
   PR_ImportFileMapFromString(
     const char *fmstring
   );

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

fmstring
   A pointer to string created by ``PR_ExportFileMapAsString``.

.. _Returns:

Returns
~~~~~~~

``PRFileMap`` pointer or ``NULL`` on error.

.. _Description:

Description
-----------

``PR_ImportFileMapFromString`` creates a ``PRFileMap`` object from a
string previously created by ``PR_ExportFileMapAsString``.
