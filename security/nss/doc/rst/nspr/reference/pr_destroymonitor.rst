Destroys a monitor object.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prmon.h>

   void PR_DestroyMonitor(PRMonitor *mon);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type ``PRMonitor``.

.. _Description:

Description
-----------

The caller is responsible for guaranteeing that the monitor is no longer
in use before calling ``PR_DestroyMonitor``. There must be no thread
(including the calling thread) in the monitor or waiting on the monitor.
