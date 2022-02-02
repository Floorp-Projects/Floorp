PR_CreateThreadPool
===================

Create a new hash table.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtpool.h>

   NSPR_API(PRThreadPool *)
   PR_CreateThreadPool(
     PRInt32 initial_threads,
     PRInt32 max_threads,
     PRUint32 stacksize
   );

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``initial_threads``
   The number of threads to be created within this thread pool.
``max_threads``
   The limit on the number of threads that will be created to server the
   thread pool.
``stacksize``
   Size of the stack allocated to each thread in the thread.

.. _Returns:

Returns
~~~~~~~

Pointer to a :ref:`PRThreadPool` structure or ``NULL`` on error.

.. _Description:

Description
~~~~~~~~~~~
