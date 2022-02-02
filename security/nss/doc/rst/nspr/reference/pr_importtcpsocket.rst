Imports a native TCP socket into NSPR.

.. _Syntax:

Syntax
------

.. code:: eval

   #include "private/pprio.h"

   PRFileDesc* PR_ImportTCPSocket(PROsfd osfd);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``osfd``
   The native file descriptor for the TCP socket to import. On POSIX
   systems, this is an ``int``. On Windows, this is a ``SOCKET``.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion, a pointer to the ``PRFileDesc`` object
   created for the newly imported native TCP socket.
-  If the import of the native TCP socket failed, ``NULL``.

.. _Description:

Description
-----------

A native TCP socket ``osfd`` can be imported into NSPR with
``PR_ImportTCPSocket``. The caller gives up control of the native TCP
socket ``osfd`` and should use the ``PRFileDesc*`` returned by
``PR_ImportTCPSocket`` instead.

Although ``PR_ImportTCPSocket`` is a supported function, it is declared
in ``"private/pprio.h"`` to stress the fact that this function depends
on the internals of the NSPR implementation. The caller needs to
understand what NSPR will do to the native file descriptor and make sure
that NSPR can use the native file descriptor successfully.

For example, on POSIX systems, NSPR will put the native file descriptor
(an ``int``) in non-blocking mode by calling ``fcntl`` to set the
``O_NONBLOCK`` file status flag on the native file descriptor, and then
NSPR will call socket functions such as ``recv``, ``send``, and ``poll``
on the native file descriptor. The caller must not do anything to the
native file descriptor before the ``PR_ImportTCPSocket`` call that will
prevent the native file descriptor from working in non-blocking mode.

.. _Warning:

Warning
-------

In theory, code that uses ``PR_ImportTCPSocket`` may break when NSPR's
implementation changes. In practice, this is unlikely to happen because
NSPR's implementation has been stable for years and because of NSPR's
strong commitment to backward compatibility. Using
``PR_ImportTCPSocket`` is much more convenient than writing an NSPR I/O
layer that wraps your native TCP sockets. Of course, it is best if you
just use ``PR_OpenTCPSocket`` or ``PR_NewTCPSocket``. If you are not
sure whether ``PR_ImportTCPSocket`` is right for you, please ask in the
mozilla.dev.tech.nspr newsgroup.
