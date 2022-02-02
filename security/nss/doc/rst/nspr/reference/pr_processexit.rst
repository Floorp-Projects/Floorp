PR_ProcessExit
==============

Causes an immediate, nongraceful, forced termination of the process.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prinit.h>

   void PR_ProcessExit(PRIntn status);

.. _Parameter:

Parameter
~~~~~~~~~

:ref:`PR_ProcessExit` has one parameter:

status
   The exit status code of the process.
