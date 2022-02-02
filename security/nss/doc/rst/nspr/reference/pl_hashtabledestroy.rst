Frees the table and all the entries.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   void PL_HashTableDestroy(PLHashTable *ht);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``ht``
   A pointer to the hash table to be destroyed.

.. _Description:

Description
-----------

``PL_HashTableDestroy`` frees all the entries in the table and the table
itself. The entries are freed by the ``freeEntry`` function (with the
``HT_FREE_ENTRY`` flag) in the ``allocOps`` structure supplied when the
table was created.
