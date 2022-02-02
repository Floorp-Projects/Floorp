.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   typedef PLHashNumber (PR_CALLBACK *PLHashFunction)(const void *key);

.. _Description:

Description
-----------

``PLHashNumber`` is a function type that maps the key of a hash table
entry to a hash number.

.. _See_Also:

See Also
--------

`PL_HashString <PL_HashString>`__
