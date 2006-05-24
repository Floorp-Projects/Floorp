.. title:: MochiKit.Iter - itertools for JavaScript; iteration made HARD, and then easy

Name
====

MochiKit.Iter - itertools for JavaScript; iteration made HARD, and then easy


Synopsis
========

::

        
    theSum = sum(takewhile(
            partial(operator.gt, 10),
            imap(
                partial(operator.mul, 2),
                count()
            )
        )
    ));

    assert( theSum == (0 + 0 + 2 + 4 + 6 + 8) );


Description
===========

All of the functional programming missing from :mochiref:`MochiKit.Base` lives
here. The functionality in this module is largely inspired by Python's iteration
protocol [1]_, and the itertools module [2]_.

MochiKit.Iter defines a standard way to iterate over anything, that you can
extend with :mochiref:`registerIterator`, or by implementing the ``.iter()``
protocol. Iterators are lazy, so it can potentially be cheaper to build a
filter chain of iterators than to build lots of intermediate arrays.
Especially when the data set is very large, but the result is not.


Dependencies
============

- :mochiref:`MochiKit.Base`


Overview
========

Iteration for JavaScript
------------------------

The best overview right now is in my Iteration for JavaScript [3]_ blog entry.
This information will migrate here eventually.

API Reference
=============

Errors
------

:mochidef:`StopIteration`:

    The singleton :mochiref:`MochiKit.Base.NamedError` that signifies the end
    of an iterator

Functions
---------

:mochidef:`applymap(fun, seq[, self])`:
    
    ``applymap(fun, seq)`` -->
        fun.apply(self, seq0), fun.apply(self, seq1), ...


:mochidef:`chain(p, q[, ...])`:

    ``chain(p, q, ...)`` --> p0, p1, ... plast, q0, q1, ...


:mochidef:`count(n=0)`:

    ``count(n=0)`` --> n, n + 1, n + 2, ...


:mochidef:`cycle(p)`:

    ``cycle(p)`` --> p0, p1, ... plast, p0, p1, ...


:mochidef:`dropwhile(pred, seq)`:

    ``dropwhile(pred, seq)`` --> seq[n], seq[n + 1], starting when
        pred(seq[n]) fails


:mochidef:`every(iterable, func)`:

    Return ``true`` if ``func(item)`` is ``true`` for every item in
    ``iterable``.


:mochidef:`exhaust(iterable)`:

    Exhausts an iterable without saving the results anywhere,
    like :mochiref:`list(iterable)` when you don't care what the output is.


:mochidef:`forEach(iterable, func[, self])`:

    Call ``func`` for each item in ``iterable``, and don't save the results.


:mochidef:`groupby(iterable[, keyfunc])`:

    Make an iterator that returns consecutive keys and groups from the
    iterable. The key is a function computing a key value for each element.
    If not specified or is None, key defaults to an identity function and
    returns the element unchanged. Generally, the iterable needs to already be
    sorted on the same key function.

    The returned group is itself an iterator that shares the underlying 
    iterable with :mochiref:`groupby()`. Because the source is shared, when the
    groupby object is advanced, the previous group is no longer visible.
    So, if that data is needed later, it should be stored as an array::

        var groups = [];
        var uniquekeys = [];
        forEach(groupby(data, keyfunc), function (key_group) {
            groups.push(list(key_group[1]));
            uniquekeys.push(key_group[0]);
        });

    As a convenience, :mochiref:`groupby_as_array()` is provided to suit the above
    use case.


:mochidef:`groupby_as_array(iterable[, keyfunc])`:

    Perform the same task as :mochiref:`groupby()`, except return an array of
    arrays instead of an iterator of iterators.


:mochidef:`iextend(lst, iterable)`:

    Just like :mochiref:`list(iterable)`, except it pushes results on ``lst``
    rather than creating a new one.


:mochidef:`ifilter(pred, seq)`:

    ``ifilter(pred, seq)`` --> elements of seq where ``pred(elem)`` is ``true``


:mochidef:`ifilterfalse(pred, seq)`:

    ``ifilterfalse(pred, seq)`` --> elements of seq where ``pred(elem)`` is
        ``false``
 

:mochidef:`imap(fun, p, q[, ...])`:

    ``imap(fun, p, q, ...)`` --> fun(p0, q0, ...), fun(p1, q1, ...), ...
    

:mochidef:`islice(seq, [start,] stop[, step])`:

    ``islice(seq, [start,] stop[, step])`` --> elements from 
        seq[start:stop:step] (in Python slice syntax)


:mochidef:`iter(iterable[, sentinel])`:

    Convert the given argument to an iterator (object implementing
    ``.next()``).
    
    1. If ``iterable`` is an iterator (implements ``.next()``), then it will
       be returned as-is.
    2. If ``iterable`` is an iterator factory (implements ``.iter()``), then
       the result of ``iterable.iter()`` will be returned.
    3. Otherwise, the iterator factory :mochiref:`MochiKit.Base.AdapterRegistry`
       is used to find a match.
    4. If no factory is found, it will throw ``TypeError``

    Built-in iterator factories are present for Array-like objects, and
    objects that implement the ``iterateNext`` protocol (e.g. the result of
    Mozilla's ``document.evaluate``).

    When used directly, using an iterator should look like this::

        var it = iter(iterable);
        try {
            while (var o = it.next()) {
                // use o
            }
        } catch (e) {
            if (e != StopIteration) {
                throw e;
            }
            // pass
        }

    This is ugly, so you should use the higher order functions to work
    with iterators whenever possible.


:mochidef:`izip(p, q[, ...])`:

    ``izip(p, q, ...)`` --> [p0, q0, ...], [p1, q1, ...], ...


:mochidef:`list(iterable)`:

    Convert ``iterable`` to a new ``Array``


:mochidef:`next(iterator)`:

    Return ``iterator.next()``


:mochidef:`range([start,] stop[, step])`:

    Return an iterator containing an arithmetic progression of integers.

    ``range(i, j)`` returns :mochiref:`iter([i, i + 1, i + 2, ..., j - 1])`

    ``start`` (!) defaults to ``0``. When ``step`` is given, it specifies the
    increment (or decrement). The end point is omitted!
    
    For example, ``range(4)`` returns :mochiref:`iter([0, 1, 2, 3])`.
    This iterates over exactly the valid indexes for an array of 4 elements.
        

:mochidef:`reduce(fn, iterable[, initial])`:

    Apply ``fn(a, b)`` cumulatively to the items of an
    iterable from left to right, so as to reduce the iterable
    to a single value.

    For example::
    
        reduce(function (a, b) { return x + y; }, [1, 2, 3, 4, 5])

    calculates::

        ((((1 + 2) + 3) + 4) + 5).
    
    If initial is given, it is placed before the items of the sequence
    in the calculation, and serves as a default when the sequence is
    empty.

    Note that the above example could be written more clearly as::

        reduce(operator.add, [1, 2, 3, 4, 5])

    Or even simpler::

        sum([1, 2, 3, 4, 5])


:mochidef:`registerIteratorFactory(name, check, iterfactory[, override])`:

    Register an iterator factory for use with the iter function.

    ``check`` is a ``function(a)`` that returns ``true`` if ``a`` can be
    converted into an iterator with ``iterfactory``.

    ``iterfactory`` is a ``function(a)`` that returns an object with a
    ``.next()`` method that returns the next value in the sequence.

    ``iterfactory`` is guaranteed to only be called if ``check(a)``
    returns a true value.

    If ``override`` is ``true``, then it will be made the
    highest precedence iterator factory. Otherwise, the lowest.


:mochidef:`repeat(elem[, n])`:

    ``repeat(elem, [,n])`` --> elem, elem, elem, ... endlessly or up to n times
        

:mochidef:`reversed(iterable)`:

    Return a reversed array from iterable.


:mochidef:`some(iterable, func)`:

    Return ``true`` if ``func(item)`` is ``true`` for at least one item in
    ``iterable``.


:mochidef:`sorted(iterable[, cmp])`:

    Return a sorted array from iterable.


:mochidef:`sum(iterable, start=0)`:

    Returns the sum of a sequence of numbers plus the value
    of parameter ``start`` (with a default of 0). When the sequence is
    empty, returns start.

    Equivalent to::

        reduce(operator.add, iterable, start);
        

:mochidef:`takewhile(pred, seq)`:

    ``takewhile(pred, seq)`` --> seq[0], seq[1], ... until pred(seq[n]) fails


:mochidef:`tee(iterable, n=2)`:

    ``tee(it, n=2)`` --> [it1, it2, it3, ... itn] splits one iterator into n


See Also
========

.. [1] The iteration protocol is described in 
       PEP 234 - Iterators: http://www.python.org/peps/pep-0234.html
.. [2] Python's itertools
       module: http://docs.python.org/lib/module-itertools.html
.. [3] Iteration in JavaScript: http://bob.pythonmac.org/archives/2005/07/06/iteration-in-javascript/


Authors
=======

- Bob Ippolito <bob@redivi.com>


Copyright
=========

Copyright 2005 Bob Ippolito <bob@redivi.com>. This program is dual-licensed
free software; you can redistribute it and/or modify it under the terms of the
`MIT License`_ or the `Academic Free License v2.1`_.

.. _`MIT License`: http://www.opensource.org/licenses/mit-license.php
.. _`Academic Free License v2.1`: http://www.opensource.org/licenses/afl-2.1.php
