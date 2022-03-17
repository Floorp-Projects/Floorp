XPCOM Hashtable Guide
=====================

.. note::

   For a deep-dive into the underlying mechanisms that power our hashtables,
   check out the :ref:`XPCOM Hashtable Technical Details`
   document.

What Is a Hashtable?
--------------------

A hashtable is a data construct that stores a set of **items**. Each
item has a **key** that identifies the item. Items are found, added, and
removed from the hashtable by using the key. Hashtables may seem like
arrays, but there are important differences:

+-------------------------+----------------------+----------------------+
|                         | Array                | Hashtable            |
+=========================+======================+======================+
| **Keys**                | *integer:* arrays    | *any type:* almost   |
|                         | are always keyed on  | any datatype can be  |
|                         | integers and must    | used as key,         |
|                         | be contiguous.       | including strings,   |
|                         |                      | integers, XPCOM      |
|                         |                      | interface pointers,  |
|                         |                      | IIDs, and almost     |
|                         |                      | anything else. Keys  |
|                         |                      | can be disjunct      |
|                         |                      | (i.e. you can store  |
|                         |                      | entries with keys 1, |
|                         |                      | 5, and 3000).        |
+-------------------------+----------------------+----------------------+
| **Lookup Time**         | *O(1):* lookup time  | *O(1):* lookup time  |
|                         | is a simple constant | is mostly-constant,  |
|                         |                      | but the constant     |
|                         |                      | time can be larger   |
|                         |                      | than an array lookup |
+-------------------------+----------------------+----------------------+
| **Sorting**             | *sorted:* stored     | *unsorted:* stored   |
|                         | sorted; iterated     | unsorted; cannot be  |
|                         | over in a sorted     | iterated over in a   |
|                         | fashion.             | sorted manner.       |
+-------------------------+----------------------+----------------------+
| **Inserting/Removing**  | *O(n):* adding and   | *O(1):* adding and   |
|                         | removing items from  | removing items from  |
|                         | a large array can be | hashtables is a      |
|                         | time-consuming       | quick operation      |
+-------------------------+----------------------+----------------------+
| **Wasted space**        | *none:* Arrays are   | *some:* hashtables   |
|                         | packed structures,   | are not packed       |
|                         | so there is no       | structures;          |
|                         | wasted space.        | depending on the     |
|                         |                      | implementation,      |
|                         |                      | there may be         |
|                         |                      | significant wasted   |
|                         |                      | memory.              |
+-------------------------+----------------------+----------------------+

In their implementation, hashtables take the key and apply a
mathematical **hash function** to **randomize** the key and then use the
hash to find the location in the hashtable. Good hashtable
implementations will automatically resize the hashtable in memory if
extra space is needed, or if too much space has been allocated.

.. _When_Should_I_Use_a_Hashtable.3F:

When Should I Use a Hashtable?
------------------------------

Hashtables are useful for

-  sets of data that need swift **random access**
-  with **non-integral keys** or **non-contiguous integral keys**
-  or where **items will be frequently added or removed**

Hashtables should *not* be used for

-  Sets that need to be **sorted**
-  Very small datasets (less than 12-16 items)
-  Data that does not need random access

In these situations, an array, a linked-list, or various tree data
structures are more efficient.

.. _Which_Hashtable_Should_I_Use.3F:

Which Hashtable Should I Use?
-----------------------------

If there is **no** key type, you should use an ``nsTHashSet``.

If there is a key type, you should use an ``nsTHashMap``.

``nsTHashMap`` is a template with two parameters. The first is the hash key
and the second is the data to be stored as the value in the map. Most of
the time, you can simply pass the raw key type as the first parameter,
so long as its supported by `nsTHashMap.h <https://searchfox.org/mozilla-central/source/xpcom/ds/nsTHashMap.h>`_.
It is also possible to specify custom keys if necessary. See `nsHashKeys.h
<https://searchfox.org/mozilla-central/source/xpcom/ds/nsHashKeys.h>`_ for examples.

There are a number of more esoteric hashkey classes in nsHashKeys.h, and
you can always roll your own if none of these fit your needs (make sure
you're not duplicating an existing hashkey class though!)

Once you've determined what hashtable and hashkey classes you need, you
can put it all together. A few examples:

-  A hashtable that maps UTF-8 origin names to a DOM Window -
   ``nsTHashMap<nsCString, nsCOMPtr<nsIDOMWindow>>``
-  A hashtable that maps 32-bit integers to floats -
   ``nsTHashMap<uint32_t, float>``
-  A hashtable that maps ``nsISupports`` pointers to reference counted
   ``CacheEntry``\ s -
   ``nsTHashMap<nsCOMPtr<nsISupports>, RefPtr<CacheEntry>>``
-  A hashtable that maps ``JSContext`` pointers to a ``ContextInfo``
   struct - ``nsTHashMap<JSContext*, UniquePtr<ContextInfo>>``
-  A hashset of strings - ``nsTHashSet<nsString>``

.. _nsBaseHashtable_and_friends:_nsDataHashtable.2C_nsInterfaceHashtable.2C_and_nsClassHashtable:

Hashtable API
-------------

The hashtable classes all expose the same basic API. There are three
key methods, ``Get``, ``InsertOrUpdate``, and ``Remove``, which retrieve entries from the
hashtable, write entries into the hashtable, and remove entries from the
hashtable respectively. See `nsBaseHashtable.h <https://searchfox.org/mozilla-central/source/xpcom/ds/nsBaseHashtable.h>`_
for more details.

The hashtables that hold references to pointers (nsRefPtrHashtable and
nsInterfaceHashtable) also have GetWeak methods that return non-AddRefed
pointers.

Note that ``nsRefPtrHashtable``, ``nsInterfaceHashtable`` and ``nsClassHashtable``
are legacy hashtable types which have some extra methods, and don't have automatic
key type handling.

All of these hashtable classes can be iterated over via the ``Iterator``
class, with normal C++11 iterators or using the ``Keys()`` / ``Values()`` ranges,
and all can be cleared via the ``Clear`` method.
