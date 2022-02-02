This chapter describes the hash table functions in the plds (portable
library — data structures) library of NSPR. The hash table library
functions are declared in the header file ``plhash.h.``

.. warning::

   **Warning**: The NSPR hash table library functions are not thread
   safe.

A hash table lookup may change the internal organization of the hash
table (to speed up future lookups).

-  `Hash Table Types and Constants <#Hash_Table_Types_and_Constants>`__
-  `Hash Table Functions <#Hash_Table_Functions>`__

.. _Hash_Table_Types_and_Constants:

Hash Table Types and Constants
------------------------------

-  ``PLHashEntry``
-  ``PLHashTable``
-  ``PLHashNumber``
-  ``PLHashFunction``
-  ``PLHashComparator``
-  ``PLHashEnumerator``
-  ``PLHashAllocOps``

.. _Hash_Table_Functions:

Hash Table Functions
--------------------

-  ``PL_NewHashTable``
-  ``PL_HashTableDestroy``
-  ``PL_HashTableAdd``
-  ``PL_HashTableRemove``
-  ``PL_HashTableLookup``
-  ``PL_HashTableEnumerateEntries``
-  ``PL_HashString``
-  ``PL_CompareStrings``
-  ``PL_CompareValues``

.. _See_also:

See also
--------

-  `XPCOM hashtable guide </en-US/docs/XPCOM_hashtable_guide>`__
