Create a pollable event file descriptor.

.. _Syntax:

Syntax
------

.. code:: eval

   NSPR_API(PRFileDesc *) PR_NewPollableEvent( void);

.. _Parameter:

Parameter
~~~~~~~~~

None.

.. _Returns:

Returns
~~~~~~~

Pointer to ``PRFileDesc`` or ``NULL``, on error.
