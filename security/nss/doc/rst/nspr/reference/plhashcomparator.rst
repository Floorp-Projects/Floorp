.. _Syntax:

Syntax
------

.. code:: eval

   #include <plhash.h>

   typedef PRIntn (PR_CALLBACK *PLHashComparator)(
     const void *v1,
     const void *v2);

.. _Description:

Description
-----------

``PLHashComparator`` is a function type that compares two values of an
unspecified type. It returns a nonzero value if the two values are
equal, and 0 if the two values are not equal. ``PLHashComparator``
defines the meaning of equality for the unspecified type.

For convenience, two comparator functions are provided.
``PL_CompareStrings`` compare two character strings using ``strcmp``.
``PL_CompareValues`` compares the values of the arguments v1 and v2
numerically.

.. _Remark:

Remark
------

The return value of ``PLHashComparator`` functions should be of type
``PRBool``.

.. _See_Also:

See Also
--------

``PL_CompareStrings``, ``PL_CompareValues``
