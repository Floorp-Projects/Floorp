Returns the current thread object for the currently running code.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prthread.h>

   PRThread* PR_GetCurrentThread(void);

.. _Returns:

Returns
~~~~~~~

Always returns a valid reference to the calling thread--a self-identity.

.. _Description:

Description
~~~~~~~~~~~

The currently running thread may discover its own identity by calling
``PR_GetCurrentThread``.

.. note::

   **Note**: This is the only safe way to establish the identity of a
   thread. Creation and enumeration are both subject to race conditions.
