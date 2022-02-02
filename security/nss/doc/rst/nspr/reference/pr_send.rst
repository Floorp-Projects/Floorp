Sends bytes from a connected socket.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRInt32 PR_Send(
     PRFileDesc *fd,
     const void *buf,
     PRInt32 amount,
     PRIntn flags,
     PRIntervalTime timeout);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a ``PRFileDesc`` object representing a socket.
``buf``
   A pointer to a buffer containing the data to be sent.
``amount``
   The size of ``buf`` (in bytes).
``flags``
   This obsolete parameter must always be zero.
``timeout``
   A value of type ``PRIntervalTime`` specifying the time limit for
   completion of the receive operation.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  A positive number indicates the number of bytes successfully sent. If
   the parameter fd is a blocking socket, this number must always equal
   amount.
-  The value -1 indicates a failure. The reason for the failure can be
   obtained by calling ``PR_GetError``.

.. _Description:

Description
-----------

``PR_Send`` blocks until all bytes are sent, a timeout occurs, or an
error occurs.
