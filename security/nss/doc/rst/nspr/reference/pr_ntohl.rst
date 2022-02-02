Performs 32-bit conversion from network byte order to host byte order.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRUint32 PR_ntohl(PRUint32 conversion);

.. _Parameter:

Parameter
~~~~~~~~~

The function has the following parameter:

``conversion``
   The 32-bit unsigned integer, in network byte order, to be converted.

.. _Returns:

Returns
~~~~~~~

The value of the ``conversion`` parameter in host byte order.
