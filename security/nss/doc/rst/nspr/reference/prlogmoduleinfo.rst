The ``PRLogModuleInfo`` structure controls logging from within your
application. To log your program's activity, create a
``PRLogModuleInfo`` structure using
```PR_NewLogModule`` <http://www-archive.mozilla.org/projects/nspr/reference/html/prlog.html#25372>`__
. 

.. _Syntax:

Syntax
------

::

   #include <prlog.h>

   typedef struct PRLogModuleInfo {
      const char *name;
      PRLogModuleLevel level;
      struct PRLogModuleInfo *next;
   } PRLogModuleInfo;
