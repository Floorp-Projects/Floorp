.. container:: blockIndicator obsolete obsoleteHeader

   | **Obsolete**
   | This feature is obsolete. Although it may still work in some
     browsers, its use is discouraged since it could be removed at any
     time. Try to avoid using it.

Associates a ``PRThread`` object with an existing native thread.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <pprthread.h>

   PRThread* PR_AttachThread(
      PRThreadType type,
      PRThreadPriority priority,
      PRThreadStack *stack);

.. _Parameters:

Parameters
~~~~~~~~~~

``PR_AttachThread`` has the following parameters:

``type``
   Specifies that the thread is either a user thread
   (``PR_USER_THREAD``) or a system thread (``PR_SYSTEM_THREAD``).
``priority``
   The priority to assign to the thread being attached.
``stack``
   The stack for the thread being attached.

.. _Returns:

Returns
~~~~~~~

The function returns one of these values:

-  If successful, a pointer to a ``PRThread`` object.
-  If unsuccessful, for example if system resources are not available,
   ``NULL``.

.. _Description:

Description
-----------

You use ``PR_AttachThread`` when you want to use NSS functions on the
native thread that was not created with NSPR. ``PR_AttachThread``
informs NSPR about the new thread by associating a ``PRThread`` object
with the native thread.

The thread object is automatically destroyed when it is no longer
needed.

You don't need to call ``PR_AttachThread`` unless you create your own
native thread. ``PR_Init`` calls ``PR_AttachThread`` automatically for
the primordial thread.

.. note::

   **Note**: As of NSPR release v3.0, ``PR_AttachThread`` and
   ``PR_DetachThread`` are obsolete. A native thread not created by NSPR
   is automatically attached the first time it calls an NSPR function,
   and automatically detached when it exits.

In NSPR release 19980529B and earlier, it is necessary for a native
thread not created by NSPR to call ``PR_AttachThread`` before it calls
any NSPR functions, and call ``PR_DetachThread`` when it is done calling
NSPR functions.
