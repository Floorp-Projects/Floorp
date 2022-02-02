Returns the tail of a circular list.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prclist.h>

   PRCList *PR_LIST_TAIL (PRCList *listp);

.. _Parameter:

Parameter
~~~~~~~~~

``listp``
   A pointer to the linked list.

.. _Returns:

Returns
~~~~~~~

A pointer to a list element.

.. _Description:

Description
-----------

``PR_LIST_TAIL`` returns the tail of the specified circular list.
