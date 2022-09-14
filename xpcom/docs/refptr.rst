Reference Counting Helpers
==========================

RefPtr versus nsCOMPtr
----------------------

The general rule of thumb is to use ``nsCOMPtr<T>`` when ``T`` is an
interface type which inherits from ``nsISupports``, and ``RefPtr<T>`` when
``T`` is a concrete type.

This basic rule derives from some ``nsCOMPtr<T>`` code being factored into
the ``nsCOMPtr_base`` base class, which stores the pointer as a
``nsISupports*``. This design was intended to save some space in the binary
(though it is unclear if it still does). Since ``nsCOMPtr`` stores the
pointer as ``nsISupports*``, it must be possible to unambiguously cast from
``T*`` to ``nsISupports**``. Many concrete classes inherit from more than
one XPCOM interface, meaning that they cannot be used with ``nsCOMPtr``,
which leads to the suggestion to use ``RefPtr`` for these classes.

``nsCOMPtr<T>`` also requires that the target type ``T`` be a valid target
for ``QueryInterface`` so that it can assert that the stored pointer is a
canonical ``T`` pointer (i.e. that ``mRawPtr->QueryInterface(T_IID) ==
mRawPtr``).

do_XXX() nsCOMPtr helpers
-------------------------

There are a number of ``do_XXX`` helper methods across the codebase which can
be assigned into ``nsCOMPtr`` (and sometimes ``RefPtr``) to perform explicit
operations based on the target pointer type.

In general, when these operations succeed, they will initialize the smart
pointer with a valid value, and otherwise they will silently initialize the
smart pointer to ``nullptr``.

``do_QueryInterface`` and ``do_QueryObject``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Attempts to cast the provided object to the target class using the XPCOM
``QueryInterface`` mechanism. In general, use ``do_QueryInterface`` may only
be used to cast between interface types in a ``nsCOMPtr<T>``, and
``do_QueryObject`` in situations when downcasting to concrete types.


``do_GetInterface``
~~~~~~~~~~~~~~~~~~~

Looks up an object implementing the requested interface using the
``nsIInterfaceRequestor`` interface. If the target object doesn't implement
``nsIInterfaceRequestor`` or doesn't provide the given interface, initializes
the smart pointer with ``nullptr``.


``do_GetService``
~~~~~~~~~~~~~~~~~

Looks up the component defined by the passed-in CID or ContractID string in
the component manager, and returns a pointer to the service instance. This
may start the service if it hasn't been started already. The resulting
service will be cast to the target interface type using ``QueryInterface``.


``do_CreateInstance``
~~~~~~~~~~~~~~~~~~~~~

Looks up the component defined by the passed-in CID or ContractID string in
the component manager, creates and returns a new instance. The resulting
object will be cast to the target interface type using ``QueryInterface``.


``do_QueryReferent`` and ``do_GetWeakReference``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When passed a ``nsIWeakReference*`` (e.g. from a ``nsWeakPtr``),
``do_QueryReferent`` attempts to re-acquire a strong reference to the held
type, and cast it to the target type with ``QueryInterface``. Initializes the
smart pointer with ``nullptr`` if either of these steps fail.

In contrast ``do_GetWeakReference`` does the opposite, using
``QueryInterface`` to cast the type to ``nsISupportsWeakReference*``, and
acquire a ``nsIWeakReference*`` to the passed-in object.
