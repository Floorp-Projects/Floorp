Binds an address to a specified socket.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_Bind(
     PRFileDesc *fd,
     const PRNetAddr *addr);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a ``PRFileDesc`` object representing a socket.
``addr``
   A pointer to a ``PRNetAddr`` object representing the address to which
   the socket will be bound.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful binding of an address to a socket, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Further information can be obtained
   by calling ``PR_GetError``.

.. _Description:

Description
-----------

When a new socket is created, it has no address bound to it. ``PR_Bind``
assigns the specified address (also known as name) to the socket. If you
do not care about the exact IP address assigned to the socket, set the
``inet.ip`` field of ``PRNetAddr`` to ``PR_htonl``\ (``PR_INADDR_ANY``).
If you do not care about the TCP/UDP port assigned to the socket, set
the ``inet.port`` field of ``PRNetAddr`` to 0.

Note that if ``PR_Connect`` is invoked on a socket that is not bound, it
implicitly binds an arbitrary address the socket.

Call ``PR_GetSockName`` to obtain the address (name) bound to a socket.
