PR_Abort
========

Aborts the process in a nongraceful manner.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prinit.h>

   void PR_Abort(void);

.. _Description:

Description
-----------

:ref:`PR_Abort` results in a core file and a call to the debugger or
equivalent, in addition to causing the entire process to stop.
