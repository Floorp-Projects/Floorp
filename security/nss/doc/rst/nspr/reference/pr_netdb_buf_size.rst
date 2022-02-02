Recommended size to use when specifying a scratch buffer for
``PR_GetHostByName``, ``PR_GetHostByAddr``, ``PR_GetProtoByName``, or
``PR_GetProtoByNumber``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   #if defined(AIX) || defined(OSF1)
     #define PR_NETDB_BUF_SIZE sizeof(struct protoent_data)
   #else
     #define PR_NETDB_BUF_SIZE 1024
   #endif
