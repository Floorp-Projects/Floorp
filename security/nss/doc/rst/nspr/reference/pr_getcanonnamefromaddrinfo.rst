Extracts the canonical name of the hostname passed to
``PR_GetAddrInfoByName``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   const char *PR_GetCanonNameFromAddrInfo(const PRAddrInfo *addrInfo);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``addrInfo``
   A pointer to a ``PRAddrInfo`` structure returned by a successful call
   to ``PR_GetAddrInfoByName``.

.. _Returns:

Returns
~~~~~~~

The function returns a const pointer to the canonical hostname stored in
the given ``PRAddrInfo`` structure. This pointer is invalidated once the
``PRAddrInfo`` structure is destroyed by a call to ``PR_FreeAddrInfo``.
