XPCOM Collections
=================

``nsTArray`` and ``AutoTArray``
-------------------------------

``nsTArray<T>`` is a typesafe array for holding various objects, similar to ``std::vector<T>``. (note that
``nsTArray<T>`` is dynamically-sized, unlike ``std::array<T>``) Here's an
incomplete list of mappings between the two:

================== ==================================================
std::vector<T>     nsTArray<T>
================== ==================================================
``size()``         ``Length()``
``empty()``        ``IsEmpty()``
``resize()``       ``SetLength()`` or ``SetLengthAndRetainStorage()``
``capacity()``     ``Capacity()``
``reserve()``      ``SetCapacity()``
``push_back()``    ``AppendElement()``
``insert()``       ``AppendElements()``
``emplace_back()`` ``EmplaceBack()``
``clear()``        ``Clear()`` or ``ClearAndRetainStorage()``
``data()``         ``Elements()``
``at()``           ``ElementAt()``
``back()``         ``LastElement()``
================== ==================================================

Rust Bindings
~~~~~~~~~~~~~

When the ``thin_vec`` crate is built in Gecko, ``thin_vec::ThinVec<T>`` is
guaranteed to have the same memory layout and allocation strategy as
``nsTArray``, meaning that the two types may be used interchangeably across
FFI boundaries. The type is **not** safe to pass by-value over FFI
boundaries, due to Rust and C++ differing in when they run destructors.

The element type ``T`` must be memory-compatible with both Rust and C++ code
to use over FFI.

``nsTHashMap`` and ``nsTHashSet``
---------------------------------

These types are the recommended interface for writing new XPCOM hashmaps and
hashsets in XPCOM code.

Supported Hash Keys
~~~~~~~~~~~~~~~~~~~

The following types are supported as the key parameter to ``nsTHashMap`` and
``nsTHashSet``.

========================== ======================
Type                       Hash Key
========================== ======================
``T*``                     ``nsPtrHashKey<T>``
``T*``                     ``nsPtrHashKey<T>``
``nsCString``              ``nsCStringHashKey``
``nsString``               ``nsStringHashKey``
``uint32_t``               ``nsUint32HashKey``
``uint64_t``               ``nsUint64HashKey``
``intptr_t``               ``IntPtrHashKey``
``nsCOMPtr<nsISupports>``  ``nsISupportsHashKey``
``RefPtr<T>``              ``nsRefPtrHashKey<T>``
``nsID``                   ``nsIDHashKey``
========================== ======================

Any key not in this list must inherit from the ``PLDHashEntryHdr`` class to
implement manual hashing behaviour.

Class Reference
~~~~~~~~~~~~~~~

.. note::

    The ``nsTHashMap`` and ``nsTHashSet`` types are not declared exactly like
    this in code. This is intended largely as a practical reference.

.. cpp:class:: template<K, V> nsTHashMap<K, V>

    The ``nsTHashMap<K, V>`` class is currently defined as a thin type alias
    around ``nsBaseHashtable``. See the methods defined on that class for
    more detailed documentation.

    https://searchfox.org/mozilla-central/source/xpcom/ds/nsBaseHashtable.h

    .. cpp:function:: uint32_t Count() const

    .. cpp:function:: bool IsEmpty() const

    .. cpp:function:: bool Get(KeyType aKey, V* aData) const

        Get the value, returning a flag indicating the presence of the entry
        in the table.

    .. cpp:function:: V Get(KeyType aKey) const

        Get the value, returning a default-initialized object if the entry is
        not present in the table.

    .. cpp:function:: Maybe<V> MaybeGet(KeyType aKey) const

        Get the value, returning Nothing if the entry is not present in the table.

    .. cpp:function:: V& LookupOrInsert(KeyType aKey, Args&&... aArgs) const

    .. cpp:function:: V& LookupOrInsertWith(KeyType aKey, F&& aFunc) const

.. cpp:class:: template<K> nsTHashSet<K>

    The ``nsTHashSet<K>`` class is currently defined as a thin type alias
    around ``nsTBaseHashSet``. See the methods defined on that class for
    more detailed documentation.

    https://searchfox.org/mozilla-central/source/xpcom/ds/nsTHashSet.h
