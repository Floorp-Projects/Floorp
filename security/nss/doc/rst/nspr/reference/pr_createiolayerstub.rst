Creates a new layer.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRFileDesc* PR_CreateIOLayerStub(
     PRDescIdentity ident
     PRIOMethods const *methods);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``ident``
   The identity to be associated with the new layer.
``methods``
   A pointer to the ``PRIOMethods`` structure specifying the functions
   for the new layer.

.. _Returns:

Returns
~~~~~~~

A new file descriptor for the specified layer.

.. _Description:

Description
-----------

A new layer may be allocated by calling ``PR_CreateIOLayerStub``. The
file descriptor returned contains the pointer to the I/O methods table
provided. The runtime neither modifies the table nor tests its
correctness.

The caller should override appropriate contents of the file descriptor
returned before pushing it onto the protocol stack.
