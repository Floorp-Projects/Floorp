Gets the unique identity for the layer of the specified file descriptor.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRDescIdentity PR_GetLayersIdentity(PRFileDesc* fd);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``fd``
   A pointer to a file descriptor.

.. _Returns:

Returns
~~~~~~~

If successful, the function returns the ``PRDescIdentity`` for the layer
of the specified file descriptor.
