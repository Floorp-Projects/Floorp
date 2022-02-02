Statically initializes a circular list.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prclist.h>

   PR_INIT_STATIC_CLIST (PRCList *listp);

.. _Parameter:

Parameter
~~~~~~~~~

``listp``
   A pointer to the anchor of the linked list.

.. _Description:

Description
-----------

PR_INIT_STATIC_CLIST statically initializes the specified list to be an
empty list. For example,

::

   PRCList free_object_list = PR_INIT_STATIC_CLIST(&free_object_list);
