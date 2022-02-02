Protocol entry returned by ``PR_GetProtoByName`` and
``PR_GetProtoByNumber``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   typedef struct PRProtoEnt {
     char *p_name;
     char **p_aliases;
   #if defined(_WIN32)
     PRInt16 p_num;
   #else
     PRInt32 p_num;
   #endif
   } PRProtoEnt;

.. _Fields:

Fields
~~~~~~

The structure has the following fields:

``p_name``
   Pointer to official protocol name.
``p_aliases``
   Pointer to a pointer to a list of aliases. The list is terminated
   with a ``NULL`` entry.
``p_num``
   Protocol number.
