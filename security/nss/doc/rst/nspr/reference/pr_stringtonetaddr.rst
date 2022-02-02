Converts a character string to a network address.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRStatus PR_StringToNetAddr(
     const char *string,
     PRNetAddr *addr);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``string``
   The string to be converted.
``addr``
   On output, the equivalent network address.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. You can retrieve the reason for the
   failure by calling ``PR_GetError``.

.. _Description:

Description
-----------

For IPv4 addresses, the input string represents numbers in the Internet
standard "." notation. IPv6 addresses are indicated as strings using ":"
characters separating octets, with numerous caveats for shortcutting
(see RFC #1884). If the NSPR library and the host are configured to
support IPv6, both formats are supported. Otherwise, use of anything
other than IPv4 dotted notation results in an error.
