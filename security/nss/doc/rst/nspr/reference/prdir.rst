Directory structure used with `Directory I/O
Functions <I_O_Functions#Directory_I.2FO_Functions>`__.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   typedef struct PRDir PRDir;

.. _Description:

Description
-----------

The opaque structure ``PRDir`` represents an open directory in the file
system. The function ``PR_OpenDir`` opens a specified directory and
returns a pointer to a ``PRDir`` structure, which can be passed to
``PR_ReadDir`` repeatedly to obtain successive entries (files or
subdirectories in the open directory). To close the directory, pass the
``PRDir`` pointer to ``PR_CloseDir``.
