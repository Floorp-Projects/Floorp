Creates a new TCP socket of the specified address family.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRFileDesc* PR_OpenTCPSocket(PRIntn af);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``af``
   The address family of the new TCP socket. Can be ``PR_AF_INET``
   (IPv4), ``PR_AF_INET6`` (IPv6), or ``PR_AF_LOCAL`` (Unix domain,
   supported on POSIX systems only).

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion, a pointer to the ``PRFileDesc`` object
   created for the newly opened TCP socket.
-  If the creation of a new TCP socket failed, ``NULL``.

.. _Description:

Description
-----------

TCP (Transmission Control Protocol) is a connection-oriented, reliable
byte-stream protocol of the TCP/IP protocol suite. ``PR_OpenTCPSocket``
creates a new TCP socket of the address family ``af``. A TCP connection
is established by a passive socket (the server) accepting a connection
setup request from an active socket (the client). Typically, the server
binds its socket to a well-known port with ``PR_Bind``, calls
``PR_Listen`` to start listening for connection setup requests, and
calls ``PR_Accept`` to accept a connection. The client makes a
connection request using ``PR_Connect``.

After a connection is established, the client and server may send and
receive data between each other. To receive data, one can call
``PR_Read`` or ``PR_Recv``. To send data, one can call ``PR_Write``,
``PR_Writev``, ``PR_Send``, or ``PR_TransmitFile``. ``PR_AcceptRead`` is
suitable for use by the server to accept a new client connection and
read the client's first request in one function call.

A TCP connection can be shut down by ``PR_Shutdown``, and the sockets
should be closed by ``PR_Close``.
