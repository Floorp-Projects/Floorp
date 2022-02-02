Represents the attributes of a new process.

.. _Syntax:

Syntax
------

::

   #include <prproces.h>

   typedef struct PRProcessAttr PRProcessAttr;

.. _Description:

Description
-----------

This opaque structure describes the attributes of a process to be
created. Pass a pointer to a ``PRProcessAttr`` into ``PR_CreateProcess``
when you create a new process, specifying information such as standard
input/output redirection and file descriptor inheritance.
