Compares two ``void *`` values numerically.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   PRIntn PL_CompareValues(const
     void *v1,
     const void *v2);

.. _Description:

Description
-----------

``PL_CompareValues`` compares the two ``void *`` values ``v1`` and
``v2`` numerically, i.e., it returns the value of the expression ``v1``
== ``v2``.

``PL_CompareValues`` can be used as the comparator function for integer
or pointer-valued key or entry value.
