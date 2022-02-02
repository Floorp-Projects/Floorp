Accepts a new connection and receives a block of data.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRInt32 PR_AcceptRead(
     PRFileDesc *listenSock,
     PRFileDesc **acceptedSock,
     PRNetAddr **peerAddr,
     void *buf,
     PRInt32 amount,
     PRIntervalTime timeout);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``listenSock``
   A pointer to a ``PRFileDesc`` object representing a socket descriptor
   that has been called with the ``PR_Listen`` function, also known as
   the rendezvous socket.
``acceptedSock``
   A pointer to a pointer to a ``PRFileDesc`` object. On return,
   ``*acceptedSock`` points to the ``PRFileDesc`` object for the newly
   connected socket. This parameter is valid only if the function return
   does not indicate failure.
``peerAddr``
   A pointer a pointer to a ``PRNetAddr`` object. On return,
   ``peerAddr`` points to the address of the remote socket. The
   ``PRNetAddr`` object that ``peerAddr`` points to will be in the
   buffer pointed to by ``buf``. This parameter is valid only if the
   function return does not indicate failure.
``buf``
   A pointer to a buffer to hold data sent by the peer and the peer's
   address. This buffer must be large enough to receive ``amount`` bytes
   of data and two ``PRNetAddr`` structures (thus allowing the runtime
   to align the addresses as needed).
``amount``
   The number of bytes of data to receive. Does not include the size of
   the ``PRNetAddr`` structures. If 0, no data will be read from the
   peer.
``timeout``
   The timeout interval only applies to the read portion of the
   operation. ``PR_AcceptRead`` blocks indefinitely until the connection
   is accepted; the read will time out after the timeout interval
   elapses.

.. _Returns:

Returns
~~~~~~~

-  A positive number indicates the number of bytes read from the peer.
-  The value -1 indicates a failure. The reason for the failure can be
   obtained by calling ``PR_GetError``.

.. _Description:

Description
-----------

``PR_AcceptRead`` accepts a new connection and retrieves the newly
created socket's descriptor and the connecting peer's address. Also, as
its name suggests, ``PR_AcceptRead`` receives the first block of data
sent by the peer.
