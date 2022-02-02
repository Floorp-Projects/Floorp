Evaluates each of the possible addresses of a ``PRHostEnt`` structure,
acquired from ``PR_GetHostByName`` or ``PR_GetHostByAddr``.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prnetdb.h>

   PRIntn PR_EnumerateHostEnt(
     PRIntn enumIndex,
     const PRHostEnt *hostEnt,
     PRUint16 port,
     PRNetAddr *address);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``enumIndex``
   The index of the enumeration. To begin an enumeration, this argument
   is set to zero. To continue an enumeration (thereby getting
   successive addresses from the host entry structure), the value should
   be set to the function's last returned value. The enumeration is
   complete when a value of zero is returned.
``hostEnt``
   A pointer to a ``PRHostEnt`` structure obtained from
   ``PR_GetHostByName`` or ``PR_GetHostByAddr``.
``port``
   The port number to be assigned as part of the ``PRNetAddr``
   structure. This parameter is not checked for validity.
``address``
   On input, a pointer to a ``PRNetAddr`` structure. On output, this
   structure is filled in by the runtime if the result of the call is
   greater than 0.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If successful, the function returns the value you should specify in
   the ``enumIndex`` parameter for the next call of the enumerator. If
   the function returns 0, the enumeration is ended.
-  If unsuccessful, the function returns -1. You can retrieve the reason
   for the failure by calling ``PR_GetError``.

.. _Description:

Description
-----------

``PR_EnumerateHostEnt`` is a stateless enumerator. The principle input,
the ``PRHostEnt`` structure, is not modified.
