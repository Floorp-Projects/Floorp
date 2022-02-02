Gets the string associated with a layer's unique identity.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   const char* PR_GetNameForIdentity(PRDescIdentity ident);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``ident``
   A layer's identity.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, the function returns a pointer to the string
   associated with the specified layer.
-  If unsuccessful, the function returns ``NULL``.

.. _Description:

Description
-----------

A string may be associated with a layer when the layer is created. The
string is copied by the runtime, and ``PR_GetNameForIdentity`` returns a
pointer to that copy.
