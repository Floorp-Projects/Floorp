PR_ImportFileMapFromString
==========================

Creates a :ref:`PRFileMap` from an identifying string.

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
   A pointer to string created by :ref:`PR_ExportFileMapAsString`.

.. _Returns:

Returns
~~~~~~~

:ref:`PRFileMap` pointer or ``NULL`` on error.

.. _Description:

Description
-----------

:ref:`PR_ImportFileMapFromString` creates a :ref:`PRFileMap` object from a
string previously created by :ref:`PR_ExportFileMapAsString`.
