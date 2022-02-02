Compares two character strings.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   PRIntn PL_CompareStrings(
     const void *v1,
     const void *v2);

.. _Description:

Description
-----------

``PL_CompareStrings`` compares ``v1`` and ``v2`` as character strings
using ``strcmp``. If the two strings are equal, it returns 1. If the two
strings are not equal, it returns 0.

``PL_CompareStrings`` can be used as the comparator function for
string-valued key or entry value.
