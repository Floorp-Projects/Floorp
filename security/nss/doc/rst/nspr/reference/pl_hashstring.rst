PL_HashString
=============

A general-purpose hash function for character strings.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   PLHashNumber PL_HashString(const void *key);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``key``
   A pointer to a character string.

.. _Returns:

Returns
~~~~~~~

The hash number for the specified key.

.. _Description:

Description
-----------

:ref:`PL_HashString` can be used as the key hash function for a hash table
if the key is a character string.
