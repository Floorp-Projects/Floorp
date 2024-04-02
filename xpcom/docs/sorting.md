# Sorting and `nsTArray`

## `std::sort` and friends

Given that sorting is hard and in order to leave the hardest part to others, we base the ``nsTArray`` sort functions on ``std::sort`` and friends.

The standard sorting functions are implemented as type safe templates, allowing for aggressive compiler optimizations (mostly inlining). The std libraries provide three different sort implementations:

### ``std::sort`` or ``nsTArray<T>::Sort``

  Advantages:

  - It provides both fast average performance and (asymptotically) optimal worst-case performance.

  Caveats:

  - It may call the comparator function for the exact same element, expecting it returns ``false`` in that case ("a not lower than a");
  - The order of elements considered equal may change wrt the initial state.
  - It uses the move assignment/construction operators eventually defined on the element's class, which may have unexpected side effects if they are not trivial.

### ``std::make/sort_heap``

  We do not provide a wrapper in ``nsTArray`` for heap sort. It is assumed that ``std::sort`` is the better choice, anyways.

### ``std::stable_sort`` or ``nsTArray<T>::StableSort``

  Advantages:

  - It preserves the order of equal elements before and after sorting.

  Caveats:

  - Might try to (fallible) allocate additional temporary memory to reduce its overhead.
  - Potentially slow, O(N·(log(N)<sup>2</sup>)) if no additional memory can be used.

## Some more implementation notes

It is important to mention that different usage scenarios may still require
different approaches. While O(n*log(n)) of ``std::sort`` is optimal from an algorithmically point of view, the design of the element classes used or the frequency of calling ``Sort`` itself can influence performance significantly, in particular for higher N. Let's look at some typical use cases.

### If the list is expected to be always sorted
  - If appending of items is sparse - use ``InsertElementSorted`` which has
    O(log(N)) complexity to determine the position but might end up needing
    up to N-1 element moves on each insert (or the equivalent ``memmove``).
  - If instead you do bulk addings of N items, you should just sort once
    afterwards. Check for outer loops that might still result in multiple
    ``Sort`` calls without ever reading the array in between.
  - If the frequency of changes is high and random - consider moving away from
    ``nsTArray`` in favor of ``LinkedList``.

### If we just need a short-living, sorted representation from time to time
  - The elements are small and simple - just sort the array itself when needed.
  - Otherwise use a temporary list containing ``T*`` or ``RefPtr<T>`` or an
    index into the original array and sort that.

### If the items are held only by the list
- The items ``T`` of the list are huge and/or complex - consider using a  ``UniquePtr<T>`` list with explicit creation of the object at each insert. If the frequency of changes to the list is not very high, the overhead for the single allocations and deletes is probably bearable (and comparable to a ``LinkedList``).

### If the comparator function is expensive
- There is an easy way to transform the calculation into a scalar - precalculate it and have an additional field or an extra array for that information.

### If the sorting is expected to be stable
- Elements considered equal by the comparator are expected to keep their relative order - do not try to add clever conditions to the comparator yourself but use ``nsTArray<T>::StableSort`` (or ``std::stable_sort``) to get the best performance.

### *Remember:* The sort condition must be stable in every regard
``std::sort`` reacts  very bad on unstable conditions. The comparator must:
  - Yield the same result before and after moving an entry in the array.
  - Yield the inverse result (well, except for equality) if the input elements are just swapped.
  - Be tolerant for being called to compare the very same element with itself.
  - The comparator function itself shall not change the state of any element
    in a way that might affect the comparison results.
  - During sort execution, no other thread shall change objects in or referenced by elements in the array in a way that might affect the comparison results.

### *Remember:* Mutating operations on ``nsTArray``  are not atomic
This is also true for `Sort` and `StableSort`, of course. If you want to use the same ``nsTArray`` instance from different threads, you need to ensure the synchronization yourself.
