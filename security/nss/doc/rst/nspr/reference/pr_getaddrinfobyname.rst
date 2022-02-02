Looks up a host by name. Equivalent to ``getaddrinfo(host, NULL, ...)``
of RFC 3493.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRAddrInfo *PR GetAddrInfoByName(
     const char *hostname,
     PRUint16 af,
     PRIntn flags);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``hostname``
   The character string defining the host name of interest.
``af``
   The address family. May be ``PR_AF_UNSPEC`` or ``PR_AF_INET``.
``flags``
   May be either ``PR_AI_ADDRCONFIG`` or
   ``PR_AI_ADDRCONFIG | PR_AI_NOCANONNAME``. Include
   ``PR_AI_NOCANONNAME`` to suppress the determination of the canonical
   name corresponding to ``hostname``

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to the opaque ``PRAddrInfo`` structure
   containing the results of the host lookup. Use
   ``PR_EnumerateAddrInfo`` to inspect the ``PRNetAddr`` values stored
   in this structure. When no longer needed, this pointer must be
   destroyed with a call to ``PR_FreeAddrInfo``.
-  If unsuccessful, ``NULL``. You can retrieve the reason for the
   failure by calling ``PR_GetError``.
