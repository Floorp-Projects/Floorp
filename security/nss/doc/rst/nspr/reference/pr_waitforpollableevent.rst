Blocks the calling thread until the pollable event is set, and then
atomically unsetting the event before returning.

.. _Syntax:

Syntax
------

.. code:: eval

   NSPR_API(PRStatus) PR_WaitForPollableEvent(PRFileDesc *event);

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
