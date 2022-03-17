XPCOM Hashtable Technical Details
=================================

.. note::

   This is a deep-dive into the underlying mechanisms that power the XPCOM
   hashtables. Some of this information is quite old and may be out of date. If
   you're looking for how to use XPCOM hashtables, you should consider reading
   the :ref:`XPCOM Hashtable Guide` instead.

Mozilla's Hashtable Implementations
-----------------------------------

Mozilla has several hashtable implementations, which have been tested
and tuned, and hide the inner complexities of hashtable implementations:

-  ``PLHashTable`` - low-level C API; entry class pointers are constant;
   more efficient for large entry structures; often wastes memory making
   many small heap allocations.
-  ``nsTHashtable`` - low-level C++ wrapper around ``PLDHash``;
   generates callback functions and handles most casting automagically.
   Client writes their own entry class which can include complex key and
   data types.
-  ``nsTHashMap/nsInterfaceHashtable/nsClassHashtable`` -
   simplifies the common usage pattern mapping a simple keytype to a
   simple datatype; client does not need to declare or manage an entry class;
   ``nsTHashMap`` datatype is a scalar such as ``uint64_t``;
   ``nsInterfaceHashtable`` datatype is an XPCOM interface;
   ``nsClassHashtable`` datatype is a class pointer owned by the
   hashtable.

.. _PLHashTable:

PLHashTable
~~~~~~~~~~~

``PLHashTable`` is a part of NSPR. The header file can be found at `plhash.h
<https://searchfox.org/mozilla-central/source/nsprpub/lib/ds/plhash.h>`_.

There are two situations where ``PLHashTable`` may be preferable:

-  You need entry-pointers to remain constant.
-  The entries stored in the table are very large (larger than 12
   words).

.. _nsTHashtable:

nsTHashtable
~~~~~~~~~~~~

To use ``nsTHashtable``, you must declare an entry-class. This
entry class contains the key and the data that you are hashing. It also
declares functions that manipulate the key. In most cases, the functions
of this entry class can be entirely inline. For examples of entry classes,
see the declarations at `nsHashKeys.h
<https://searchfox.org/mozilla-central/source/xpcom/ds/nsHashKeys.h>`_.

The template parameter is the entry class. After construction, use the
functions ``PutEntry/GetEntry/RemoveEntry`` to alter the hashtable. The
``Iterator`` class will do iteration, but beware that the iteration will
occur in a seemingly-random order (no sorting).

-  ``nsTHashtable``\ s can be allocated on the stack, as class members,
   or on the heap.
-  Entry pointers can and do change when items are added to or removed
   from the hashtable. Do not keep long-lasting pointers to entries.
-  because of this, ``nsTHashtable`` is not inherently thread-safe. If
   you use a hashtable in a multi-thread environment, you must provide
   locking as appropriate.

Before using ``nsTHashtable``, see if ``nsBaseHashtable`` and relatives
will work for you. They are much easier to use, because you do not have
to declare an entry class. If you are hashing a simple key type to a
simple data type, they are generally a better choice.

.. _nsBaseHashtable_and_friends:nsTHashMap.2C_nsInterfaceHashtable.2C_and_nsClassHashtable:

nsBaseHashtable and friends: nsTHashMap, nsInterfaceHashtable, and nsClassHashtable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These C++ templates provide a high-level interface for using hashtables
that hides most of the complexities of the underlying implementation. They
provide the following features:

-  hashtable operations can be completed without using an entry class,
   making code easier to read
-  optional thread-safety: the hashtable can manage a read-write lock
   around the table
-  predefined key classes provide automatic cleanup of
   strings/interfaces
-  ``nsInterfaceHashtable`` and ``nsClassHashtable`` automatically
   release/delete objects to avoid leaks.

``nsBaseHashtable`` is not used directly; choose one of the three
derivative classes based on the data type you want to store. The
``KeyClass`` is taken from `nsHashKeys.h
<https://searchfox.org/mozilla-central/source/xpcom/ds/nsHashKeys.h>`_ and is the same for all
three classes:

-  ``nsTHashMap<KeyClass, DataType>`` - ``DataType`` is a simple
   type such as ``uint32_t`` or ``bool``.
-  ``nsInterfaceHashtable<KeyClass, Interface>`` - ``Interface`` is an
   XPCOM interface such as ``nsISupports`` or ``nsIDocShell``
-  ``nsClassHashtable<KeyClass, T>`` - ``T`` is any C++ class. The
   hashtable stores a pointer to the object, and deletes that object
   when the entry is removed.

The important files to read are
`nsBaseHashtable.h <https://searchfox.org/mozilla-central/source/xpcom/ds/nsBaseHashtable.h>`_
and
`nsHashKeys.h <https://searchfox.org/mozilla-central/source/xpcom/ds/nsHashKeys.h>`_.
These classes can be used on the stack, as a class member, or on the heap.

.. _Using_nsTHashtable_as_a_hash-set:

Using nsTHashtable as a hash-set
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A hash set only tracks the existence of keys: it does not associate data
with the keys. This can be done using ``nsTHashtable<nsSomeHashKey>``.
The appropriate entries are GetEntry and PutEntry.
