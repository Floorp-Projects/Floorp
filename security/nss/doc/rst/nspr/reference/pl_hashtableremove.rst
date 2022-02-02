Removes the entry with the specified key from the hash table.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   PRBool PL_HashTableRemove(
     PLHashTable *ht,
     const void *key);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``ht``
   A pointer to the hash table from which to remove the entry.
``key``
   A pointer to the key for the entry to be removed.

.. _Description:

Description
-----------

If there is no entry in the table with the specified key,
``PL_HashTableRemove`` returns ``PR_FALSE``. If the entry exists,
``PL_HashTableRemove`` removes the entry from the table, invokes
``freeEntry`` with the ``HT_FREE_ENTRY`` flag to frees the entry, and
returns ``PR_TRUE``.

If the table is underloaded, ``PL_HashTableRemove`` also shrinks the
number of buckets by half.

.. _Remark:

Remark
------

This function should return ``PRStatus``.
