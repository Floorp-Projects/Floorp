Initializes the runtime.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prinit.h>

   void PR_Init(
     PRThreadType type,
     PRThreadPriority priority,
     PRUintn maxPTDs);

.. _Parameters:

Parameters
~~~~~~~~~~

``PR_Init`` has the following parameters:

``type``
   This parameter is ignored.
``priority``
   This parameter is ignored.
``maxPTDs``
   This parameter is ignored.

.. _Description:

Description
-----------

NSPR is now implicitly initialized, usually by the first NSPR function
called by a program. ``PR_Init`` is necessary only if a program has
specific initialization-sequencing requirements.

Call ``PR_Init`` as follows:

.. code:: eval

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
