.. container:: blockIndicator obsolete obsoleteHeader

   | **Obsolete**
   | This feature is obsolete. Although it may still work in some
     browsers, its use is discouraged since it could be removed at any
     time. Try to avoid using it.

The opaque ``PRThreadStack`` structure is only used in the third
argument "``PRThreadStack *stack``" to the ``PR_AttachThread`` function.
The '``stack``' argument is now obsolete and ignored by
``PR_AttachThread``. You should pass ``NULL`` as the 'stack' argument to
``PR_AttachThread``.

.. _Definition:

Syntax
------

.. code:: eval

   #include <prthread.h>

   typedef struct PRThreadStack PRThreadStack;

.. _Definition_2:

 
-
