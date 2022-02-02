This chapter describes the NSPR API for managing linked lists. The API
is a set of macros for initializing a circular (doubly linked) list,
inserting and removing elements from the list. The macros are not thread
safe. The caller must provide for mutually-exclusive access to the list,
and for the nodes being added and removed from the list.

-  `Linked List Types <#Linked_List_Types>`__
-  `Linked List Macros <#Linked_List_Macros>`__

.. _Linked_List_Types:

Linked List Types
-----------------

The ``PRCList`` type represents a circular linked list.

.. _Linked_List_Macros:

Linked List Macros
------------------

Macros that create and operate on linked lists are:

-  ``PR_INIT_CLIST``
-  ``PR_INIT_STATIC_CLIST``
-  ``PR_APPEND_LINK``
-  ``PR_INSERT_LINK``
-  ``PR_NEXT_LINK``
-  ``PR_PREV_LINK``
-  ``PR_REMOVE_LINK``
-  ``PR_REMOVE_AND_INIT_LINK``
-  ``PR_INSERT_BEFORE``
-  ``PR_INSERT_AFTER``
-  ``PR_CLIST_IS_EMPTY``
-  ``PR_LIST_HEAD``
-  ``PR_LIST_TAIL``
