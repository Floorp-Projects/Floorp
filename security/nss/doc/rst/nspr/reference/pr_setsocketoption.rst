Retrieves the socket options set for a specified socket.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_SetSocketOption(
     PRFileDesc *fd,
     PRSocketOptionData *data);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a ``PRFileDesc`` object representing the socket whose
   options are to be set.
``data``
   A pointer to a structure of type ``PRSocketOptionData`` specifying
   the options to set.

.. _Returns:

Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   obtained by calling ``PR_GetError``.

.. _Description:

Description
-----------

On input, the caller must set both the ``option`` and ``value`` fields
of the ``PRSocketOptionData`` object pointed to by the ``data``
parameter.
