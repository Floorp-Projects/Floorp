Checks for an empty circular list.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prclist.h>

   PRIntn PR_CLIST_IS_EMPTY (PRCList *listp);

.. _Parameter:

Parameter
~~~~~~~~~

``listp``
   A pointer to the linked list.

.. _Description:

Description
-----------

PR_CLIST_IS_EMPTY returns a non-zero value if the specified list is an
empty list, otherwise returns zero.
