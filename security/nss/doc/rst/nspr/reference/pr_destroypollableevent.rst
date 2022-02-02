Close the file descriptor associated with a pollable event and release
related resources.

.. _Syntax:

Syntax
------

.. code:: eval

   NSPR_API(PRStatus) PR_DestroyPollableEvent(PRFileDesc *event);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``event``
   Pointer to a ``PRFileDesc`` structure previously created via a call
   to ``PR_NewPollableEvent``.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   retrieved via ``PR_GetError``.
