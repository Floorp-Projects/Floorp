PR_NewMonitor
=============

Creates a new monitor object. The caller is responsible for the object
and is expected to destroy it when appropriate.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmon.h>

   PRMonitor* PR_NewMonitor(void);

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to a :ref:`PRMonitor` object.
-  If unsuccessful (for example, if some operating system resource is
   unavailable), ``NULL``.

.. _Description:

Description
-----------

A newly created monitor has an entry count of zero.
