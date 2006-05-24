.. title:: MochiKit.Base - functional programming and useful comparisons

Name
====

MochiKit.Base - functional programming and useful comparisons


Synopsis
========

::

    myObjectRepr = function () {
        // gives a nice, stable string representation for objects,
        // ignoring any methods
        var keyValuePairs = [];
        for (var k in this) {
            var v = this[k];
            if (typeof(v) != 'function') {
                keyValuePairs.push([k, v]);
            }
        };
        keyValuePairs.sort(compare);
        return "{" + map(
            function (pair) {
                return map(repr, pair).join(":");
            }, 
            keyValuePairs
        ).join(", ") + "}";
    };
            
    // repr() will look for objects that have a repr method
    myObjectArray = [
        {"a": 3, "b": 2, "repr": myObjectRepr},
        {"a": 1, "b": 2, "repr": myObjectRepr}
    ];

    // sort it by the "a" property, check to see if it matches
    myObjectArray.sort(keyComparator("a"));
    expectedRepr = '[{"a": 1, "b": 2}, {"a": 3, "b": 2}]';
    assert( repr(myObjectArray) == expectedRepr );

    // get just the "a" values out into an array
    sortedAValues = map(itemgetter("a"), myObjectArray);
    assert( compare(sortedAValues, [1, 3]) == 0 );

    // serialize an array as JSON, unserialize it, expect something equivalent
    myArray = [1, 2, "3", null, undefined];
    assert( objEqual(evalJSON(serializeJSON(myArray)), myArray) );


Description
===========

:mochiref:`MochiKit.Base` is the foundation for the MochiKit suite.
It provides:

-   An extensible comparison facility
    (:mochiref:`compare`, :mochiref:`registerComparator`)
-   An extensible programmer representation facility
    (:mochiref:`repr`, :mochiref:`registerRepr`)
-   An extensible JSON [1]_ serialization and evaluation facility 
    (:mochiref:`serializeJSON`, :mochiref:`evalJSON`,
    :mochiref:`registerJSON`)
-   A simple adaptation facility (:mochiref:`AdapterRegistry`)
-   Convenience functions for manipulating objects and Arrays
    (:mochiref:`update`, :mochiref:`setdefault`, :mochiref:`extend`, etc.)
-   Array-based functional programming
    (:mochiref:`map`, :mochiref:`filter`, etc.)
-   Bound and partially applied functions
    (:mochiref:`bind`, :mochiref:`method`, :mochiref:`partial`)

Python users will feel at home with :mochiref:`MochiKit.Base`, as the
facilities are quite similar to those available as part of Python and the 
Python standard library.


Dependencies
============

None.


Overview
========

Comparison
----------

The comparators (operators for comparison) in JavaScript are deeply broken,
and it is not possible to teach them new tricks.

MochiKit exposes an extensible comparison facility as a simple
:mochiref:`compare(a, b)` function, which should be used in lieu of
JavaScript's operators whenever comparing objects other than numbers
or strings (though you can certainly use :mochiref:`compare` for those, too!).

The :mochiref:`compare` function has the same signature and return value as a
sort function for ``Array.prototype.sort``, and is often used in that context.

Defining new comparators for the :mochiref:`compare` function to use is done
by adding an entry to its :mochiref:`AdapterRegistry` with the
:mochiref:`registerComparator` function.


Programmer Representation
-------------------------

JavaScript's default representation mechanism, ``toString``, is notorious
for having terrible default behavior. It's also very unwise to change that
default, as other JavaScript code you may be using may depend on it.

It's also useful to separate the concept of a "string representation" and a
"string representation for programmers", much like Python does with its str
and repr protocols.

:mochiref:`repr` provides this programmer representation for JavaScript,
in a way that doesn't require object prototype hacking: using an
:mochiref:`AdapterRegistry`.

Objects that implement the repr protocol can either implement a ``.repr()``
or ``.__repr__()`` method, or they can simply have an adapter setup to
generate programmer representations. By default, the registry provides
nice representations for ``null``, ``undefined``, ``Array``, and objects or
functions with a ``NAME`` attribute that use the default ``toString``. For
objects that ``repr`` doesn't already understand, it simply defaults to
``toString``, so it will integrate seamlessly with code that implements
the idiomatic JavaScript ``toString`` method!

To define a programmer representation for your own objects, simply add
a ``.repr()`` or ``.__repr__()`` method that returns a string. For
objects that you didn't create (e.g., from a script you didn't write, or a 
built-in object), it is instead recommended that you create an adapter
with :mochiref:`registerRepr`.


JSON Serialization
------------------

JSON [1]_, JavaScript Object Notation, is a widely used serialization format
in the context of web development. It's extremely simple, lightweight, and
fast. In its essence, JSON is a restricted subset of JavaScript syntax
suitable for sending over the wire that can be unserialized with a simple
eval. It's often used as an alternative to XML in "AJAX" contexts because it
is compact, fast, and much simpler to use for most purposes.

To create a JSON serialization of any object, simply call
:mochiref:`serializeJSON()` with that object. To unserialize a JSON string,
simply call :mochiref:`evalJSON()`
with the serialization.

In order of precedence, :mochiref:`serializeJSON` coerces the given argument
into a JSON serialization:

1.  Primitive types are returned as their JSON representation: 
    ``undefined``, ``string``, ``number``, ``boolean``, ``null``.
2.  If the object has a ``__json__`` or ``json`` method, then it is called
    with no arguments. If the result of this method is not the object itself,
    then the new object goes through rule processing again (e.g. it may return
    a string, which is then serialized in JSON format).
3.  If the object is ``Array``-like (has a ``length`` property that is a
    number, and is not a function), then it is serialized as a JSON array.
    Each element will be processed according to these rules in order.
    Elements that can not be serialized (e.g. functions) will be replaced with
    ``undefined``.
4.  The ``jsonRegistry`` :mochiref:`AdapterRegistry` is consulted for an
    adapter for this object. JSON adapters take one argument (the object),
    and are expected to behave like a ``__json__`` or ``json`` method
    (return another object to be serialized, or itself).
5.  If no adapter is available, the object is enumerated and serialized as a
    JSON object (name:value pairs). All names are expected to be strings.
    Each value is serialized according to these rules, and if it can not be 
    serialized (e.g. methods), then that name:value pair will be skipped.


Adapter Registries
------------------

MochiKit makes extensive use of adapter registries, which enable you to
implement object-specific behaviors for objects that you do not necessarily
want to modify, such as built-in objects. This is especially useful because
JavaScript does not provide a method for hiding user-defined properties from
``for propName in obj`` enumeration.

:mochiref:`AdapterRegistry` is simply an encapsulation for an ordered list of
"check" and "wrap" function pairs. Each :mochiref:`AdapterRegistry` instance
should perform one function, but may have multiple ways to achieve that
function based upon the arguments. One way to think of it is as a poor man's
generic function, or multiple dispatch (on arbitrary functions, not just type!).

Check functions take one or more arguments, and return ``true`` if the
argument list is suitable for the wrap function. Check functions should
perform "cheap" checks of an object's type or contents, before the
"expensive" wrap function is called.

Wrap functions take the same arguments as check functions and do some
operation, such as creating a programmer representation or comparing
both arguments.


Convenience Functions
---------------------

Much of :mochiref:`MochiKit.Base` is there to simply remove the grunt work of
doing generic JavaScript programming.

Need to take every property from one object and set them on another? No
problem, just call :mochiref:`update(dest, src)`! What if you just wanted to
update keys that weren't already set? Look no further than
:mochiref:`setdefault(dest, src[, ...])`.

Want to return a mutable object, but don't want to suffer the consequences
if the user mutates it? Just :mochiref:`clone(it)` and you'll get a
copy-on-write clone. Cheaper than a copy!

Need to extend an ``Array`` with another array? Or even an ``Array``-like
object such as a ``NodeList`` or the special ``arguments`` object? Even if you
need to skip the first few elements of the source ``Array``-like object, it's
no problem with :mochiref:`extend(dstArray, srcArrayLike[, skip])`!

Wouldn't it be convenient to have all of the JavaScript operators were
available as functions somewhere? That's what the :mochiref:`operators` table
is for, and it even comes with additional operators based on the
:mochiref:`compare` function.

Need to walk some tree of objects and manipulate or find something in it?
A DOM element tree perhaps? Use :mochiref:`nodeWalk(node, visitor)`!

There's plenty more, so check out the `API Reference`_ below.


Functional Programming
----------------------

Functional programming constructs such as :mochiref:`map` and
:mochiref:`filter` can save you a lot of time, because JavaScript iteration is
error-prone and arduous. Writing less code is the best way to prevent bugs,
and functional programming can help you do that.

:mochiref:`MochiKit.Base` ships with a few simple Array-based functional
programming constructs, namely :mochiref:`map` and :mochiref:`filter`, and
their "extended" brethren, :mochiref:`xmap` and :mochiref:`xfilter`.

:mochiref:`map(func, arrayLike[, ...])` takes a function and an ``Array``-like
object, and creates a new ``Array``. The new ``Array`` is the result of
``func(element)`` for every element of ``arrayLike``, much
like the ``Array.prototype.map`` extension in Mozilla. However,
:mochiref:`MochiKit.Base` takes that a step further and gives you the full
blown Python version of :mochiref:`map`, which will take several
``Array``-like objects, and calls the function with one argument per given
``Array``-like, e.g.::

   var arrayOne = [1, 2, 3, 4, 5];
   var arrayTwo = [1, 5, 2, 4, 3];
   var arrayThree = [5, 2, 1, 3, 4];
   var biggestElements = map(objMax, arrayOne, arrayTwo, arrayThree);
   assert( objEqual(biggestElements, [5, 5, 3, 4, 5]) );

:mochiref:`filter(func, arrayLike[, self])` takes a function and an
``Array``-like object, and returns a new ``Array``.
This is basically identical to the ``Array.prototype.filter``
extension in Mozilla. self, if given, will be
used as ``this`` in the context of func when called.

:mochiref:`xmap` and :mochiref:`xfilter` are just special forms of
:mochiref:`map` and :mochiref:`filter` that accept a function as the first
argument, and use the extra arguments as the ``Array``-like. Not terribly
interesting, but a definite time-saver in some cases.

If you appreciate the functional programming facilities here,
you should definitely check out :mochiref:`MochiKit.Iter`, which provides
full blown iterators, :mochiref:`MochiKit.Iter.range`,
:mochiref:`MochiKit.Iter.reduce`, and a near-complete port of Python's
itertools [2]_ module, with some extra stuff thrown in for good measure!


Bound and Partial Functions
---------------------------

JavaScript's method-calling special form and lack of bound functions (functions
that know what ``this`` should be) are one of the first stumbling blocks that
programmers new to JavaScript face. The :mochiref:`bind(func, self)` method
fixes that right up by returning a new function that calls func with the right
``this``.

In order to take real advantage of all this fancy functional programming stuff,
you're probably going to want partial application. This allows you to create
a new function from an existing function that remembers some of the arguments.
For example, if you wanted to compare a given object to a slew of other 
objects, you could do something like this::

    compareWithOne = partial(compare, 1);
    results = map(compareWithOne, [0, 1, 2, 3]);
    assert( objEqual(results, [-1, 0, 1, 1]) );

One of the better uses of partial functions is in :mochiref:`MochiKit.DOM`,
which is certainly a must-see for those of you creating lots of DOM elements
with JavaScript!


API Reference
=============

Errors
------

:mochidef:`NotFound`:

    A singleton error raised when no suitable adapter is found


Constructors
------------

:mochidef:`AdapterRegistry`:
    
    A registry to facilitate adaptation.

    All ``check``/``wrap`` function pairs in a given registry
    should take the same number of arguments.


:mochidef:`AdapterRegistry.prototype.register(name, check, wrap[, override])`:

    ``name``:
        a unique identifier used to identify this adapter so that it
        may be unregistered.

    ``check``:
        function that should return ``true`` if the given arguments are
        appropriate for the ``wrap`` function.
        
    ``wrap``:
        function that takes the same parameters as ``check`` and does
        the adaptation.  Every ``wrap``/``check`` function pair in the
        registry should have the same number of arguments.

    ``override``:
        if ``true``, the ``check`` function will be
        given highest priority. Otherwise, the lowest.


:mochidef:`AdapterRegistry.prototype.match(obj[, ...])`:

    Find an adapter for the given arguments by calling every
    ``check`` function until one returns ``true``.
    
    If no suitable adapter is found, throws :mochiref:`NotFound`.


:mochidef:`AdapterRegistry.prototype.unregister(name)`:

    Remove a named adapter from the registry


:mochidef:`NamedError`:

    Convenience constructor for creating new errors (e.g. :mochiref:`NotFound`)


Functions
---------

:mochidef:`arrayEqual(self, arr)`:

    Compare the arrays ``self`` and ``arr`` for equality using ``compare``
    on each element. Uses a fast-path for length differences.


:mochidef:`bind(func, self[, arg, ...])`:

    Return a copy of ``func`` bound to ``self``. This means whenever
    and however the returned function is called, ``this`` will always
    reference the given ``self``. ``func`` may be either a function
    object, or a string. If it is a string, then ``self[func]`` will
    be used, making these two statements equivalent::
        
        bind("method", self);
        bind(self.method, self);

    Calling :mochiref:`bind(func, self)` on an already bound function will
    return a new function that is bound to the new ``self``! If
    ``self`` is ``undefined``, then the previous ``self`` is used.
    If ``self`` is ``null``, then the ``this`` object is used
    (which may or may not be the global object). To force binding
    to the global object, you should pass it explicitly.

    Additional arguments, if given, will be partially applied to
    the function. These three expressions are equivalent and
    return equally efficient functions (:mochiref:`bind` and
    :mochiref:`partial` share the same code path):

    - :mochiref:`bind(oldfunc, self, arg1, arg2)`
    - :mochiref:`bind(partial(oldfunc, arg1, arg2), self)`
    - :mochiref:`partial(bind(oldfunc, self), arg1, arg2)`


:mochidef:`bindMethods(self)`:

    Replace all functions ``meth`` on ``self`` with 
    :mochiref:`bind(meth, self)`.  This emulates
    Python's bound instance methods, where there is no
    need to worry about preserving ``this`` when the
    method is used as a callback.


:mochidef:`clone(obj)`:

    Return a new object using ``obj`` as its prototype. Use this
    if you want to return a mutable object (e.g. instance state),
    but don't want the user to mutate it. If they do, it won't
    have any effect on the original ``obj``.
    
    Note that this is a shallow clone, so mutable properties will
    have to be cloned separately if you want to "protect" them.


:mochidef:`compare(a, b)`:

    Compare two objects in a sensible manner. Currently this is:
    
    1.  ``undefined`` and ``null`` compare equal to each other
    2.  ``undefined`` and ``null`` are less than anything else
    3.  If JavaScript says ``a == b``, then we trust it
    4.  comparators registered with registerComparator are
        used to find a good comparator. Built-in comparators
        are currently available for ``Array``-like and ``Date``-like
        objects.
    5.  Otherwise hope that the built-in comparison operators
        do something useful, which should work for numbers
        and strings.
    6.  If neither ``a < b`` or ``a > b``, then throw a ``TypeError``

    Returns what one would expect from a comparison function:

    +-----------+---------------+
    | Value     | Condition     |
    +-----------+---------------+
    | ``0``     | ``a == b``    |
    +-----------+---------------+
    | ``1``     | ``a > b``     |
    +-----------+---------------+
    | ``-1``    | ``a < b``     |
    +-----------+---------------+


:mochidef:`concat(lst[, ...])`:

    Concatenates all given ``Array``-like arguments and returns
    a new ``Array``::

        var lst = concat(["1","3","5"], ["2","4","6"]);
        assert( lst.toString() == "1,3,5,2,4,6" );


:mochidef:`counter(n=1)`:

    Returns a function that will return a number one greater than
    the previous returned value, starting at ``n``. For example::

        nextId = counter()
        assert( nextId() == 1 )
        assert( nextId() == 2 )

    For an iterator with this behavior, see
    :mochiref:`MochiKit.Iter.count`.


:mochidef:`extend(self, obj, skip=0)`:

    Mutate the array ``self`` by extending it with an ``Array``-like
    ``obj``, starting from index ``skip``. If ``null`` is given
    as the initial array, a new one will be created.

    This mutates *and returns* ``self``, be warned.


:mochidef:`evalJSON(aJSONString)`:

    Unserialize a JSON [1]_ represenation of an object.
    
    Note that this uses the ``eval`` function of the interpreter, and
    therefore trusts the contents of ``aJSONString`` to be safe.
    This is acceptable when the JSON and JavaScript application
    originate from the same server, but in other scenarios it may not be the
    appropriate security model. Currently, a validating JSON parser is beyond
    the scope of MochiKit, but there is one available from json.org [1]_.


:mochidef:`filter(fn, lst)`:

    Returns a new ``Array`` composed of all elements from ``lst`` where
    ``fn(lst[i])`` returns a true value.

    If ``fn`` is ``null``, ``operator.truth`` will be used.


:mochidef:`findValue(lst, value, start=0, end=lst.length)`:

    Finds the index of ``value`` in the ``Array``-like object ``lst`` using
    :mochiref:`compare`. The search starts at the index ``start``, and ends
    at the index ``end - 1``. If ``value`` is not found in ``lst``, it will
    return ``-1``.

    For example::
    
        assert( findValue([1, 2, 3, 2, 1], 2) == 1 )
        assert( findValue([1, 2, 3, 2, 1], 2, 2) == 3 )


:mochidef:`findIdentical(lst, value, start=0, end=lst.length)`:

    Finds the index of ``value`` in the ``Array``-like object ``lst`` using
    the ``===`` operator. The search starts at the index ``start``, and ends
    at the index ``end - 1``. If ``value`` is not found in ``lst``, it will
    return ``-1``.
    
    You should use this function instead of :mochiref:`findValue` if ``lst`` may
    be comprised of objects for which no comparator is defined and all you care
    about is finding an identical object (e.g. the same instance), or if
    ``lst`` is comprised of just numbers or strings and performance is
    important.

    For example::
    
        assert( findIdentical([1, 2, 3, 2, 1], 2) == 1 )
        assert( findIdentical([1, 2, 3, 2, 1], 2, 2) == 3 )


:mochidef:`flattenArguments(arg[, ...])`:

    Given a bunch of arguments, return a single ``Array`` containing all
    of those arguments. Any ``Array`` argument will be extended in-place,
    e.g.::

        compare(flattenArguments(1, [2, 3, [4, 5]]), [1, 2, 3, 4, 5]) == 0


:mochidef:`forwardCall(name)`:

    Returns a function that forwards a method call to ``this.name(...)``


:mochidef:`isArrayLike(obj[, ...])`:

    Returns ``true`` if all given arguments are ``Array``-like (have a
    ``.length`` property and ``typeof(obj) == 'object'``)


:mochidef:`isDateLike(obj[, ...])`:

    Returns ``true`` if all given arguments are ``Date``-like (have a 
    ``.getTime()`` method)


:mochidef:`isEmpty(obj[, ...])`:

    Returns ``true`` if all the given ``Array``-like or string arguments
    are empty ``(obj.length == 0)``


:mochidef:`isNotEmpty(obj[, ...])`:

    Returns ``true`` if all the given ``Array``-like or string arguments
    are not empty ``(obj.length > 0)``


:mochidef:`isNull(obj[, ...])`:

    Returns ``true`` if all arguments are ``null``.


:mochidef:`isUndefinedOrNull(obj[, ...])`:

    Returns ``true`` if all arguments are undefined or ``null``


:mochidef:`itemgetter(name)`:

    Returns a ``function(obj)`` that returns ``obj[name]``


:mochidef:`items(obj)`:

    Return an ``Array`` of ``[propertyName, propertyValue]`` pairs for the
    given ``obj`` (in the order determined by ``for propName in obj``).


:mochidef:`keyComparator(key[, ...])`:

    A comparator factory that compares ``a[key]`` with ``b[key]``.
    e.g.::

        var lst = ["a", "bbb", "cc"];
        lst.sort(keyComparator("length"));
        assert( lst.toString() == "a,cc,bbb" );


:mochidef:`keys(obj)`:

    Return an ``Array`` of the property names of an object
    (in the order determined by ``for propName in obj``).


:mochidef:`listMax(lst)`:

    Return the largest element of an ``Array``-like object, as determined
    by :mochiref:`compare`. This is a special form of :mochiref:`listMinMax`,
    specifically :mochiref:`partial(listMinMax, 1)`.


:mochidef:`listMin(lst)`:

    Return the smallest element of an ``Array``-like object, as determined
    by :mochiref:`compare`. This is a special form of :mochiref:`listMinMax`,
    specifically :mochiref:`partial(listMinMax, -1)`.


:mochidef:`listMinMax(which, lst)`:

    If ``which == -1`` then it will return the smallest
    element of the ``Array``-like ``lst``. This is also available
    as :mochiref:`listMin(lst)`.

    If ``which == 1`` then it will return the largest
    element of the ``Array``-like ``lst``. This is also available
    as :mochiref:`listMax(list)`.


:mochidef:`map(fn, lst[, ...])`:

    Return a new array composed of the results of ``fn(x)`` for every ``x``
    in ``lst``.

    If ``fn`` is ``null``, and only one sequence argument is given the
    identity function is used.
    
        :mochiref:`map(null, lst)` -> ``lst.slice()``;

    If ``fn`` is ``null``, and more than one sequence is given as arguments,
    then the ``Array`` function is used, making it equivalent to
    :mochiref:`zip`.

        :mochiref:`map(null, p, q, ...)`
            -> :mochiref:`zip(p, q, ...)`
            -> ``[[p0, q0, ...], [p1, q1, ...], ...];``


:mochidef:`merge(obj[, ...])`:

    Create a new instance of ``Object`` that contains every property
    from all given objects. If a property is defined on more than
    one of the objects, the last property is used.

    This is a special form of :mochiref:`update(self, obj[, ...])`,
    specifically, it is defined as :mochiref:`partial(update, null)`.


:mochidef:`method(self, func, ...)`:

    Alternate form of :mochiref:`bind` that takes the object before the
    function. These two calls are equivalent::

        bind("method", myobject)
        method(myobject, "method")


:mochidef:`nameFunctions(namespace)`:

    Given a ``namespace`` (object or function) with a ``NAME`` property,
    find all methods in it and give them nice ``NAME`` properties too
    (for use with :mochiref:`repr`). e.g.::

        namespace = {
            NAME: "Awesome",
            Dude: function () {}
        }
        nameFunctions(namespace);
        assert( namespace.Dude.NAME == 'Awesome.Dude' );


:mochidef:`objEqual(a, b)`:

    Return ``true`` if ``compare(a, b) == 0``
    

:mochidef:`nodeWalk(node, visitor)`:

    Non-recursive generic node walking function (e.g. for a DOM).

    The walk order for nodeWalk is breadth first, meaning that all
    siblings will be visited before any children.

    ``node``:
        The initial node to be searched.

    ``visitor``:
        The visitor function, will be called as
        ``visitor(node)``, and should return an ``Array``-like
        of nodes to be searched next (e.g. ``node.childNodes``).
        Leaf nodes may return ``null`` or ``undefined``.


:mochidef:`objMax(obj[, ...])`:

    Return the maximum object according to :mochiref:`compare` out
    of the given arguments. This is similar to :mochiref:`listMax`,
    except is uses the arguments instead of a given ``Array``-like.
        

:mochidef:`objMin(obj[, ...])`:

    Return the minimum object according to :mochiref:`compare`
    out of the given arguments. This is similar to
    :mochiref:`listMin`, except it uses the arguments instead of a
    given ``Array``-like.


:mochidef:`operator`:

    A table of JavaScript's operators for usage with :mochiref:`map`,
    :mochiref:`filter`, etc.


    Unary Logic Operators:

        +--------------------+--------------------------+-------------------+
        | Operator           | Implementation           | Description       |
        +====================+==========================+===================+
        | ``truth(a)``       | ``!!a``                  | Logical truth     |
        +--------------------+--------------------------+-------------------+
        | ``lognot(a)``      | ``!a``                   | Logical not       |
        +--------------------+--------------------------+-------------------+
        | ``identity(a)``    | ``a``                    | Logical identity  |
        +--------------------+--------------------------+-------------------+



    Unary Math Operators: 

        +--------------------+--------------------------+---------------+
        | Operator           | Implementation           | Description   |
        +====================+==========================+===============+
        | ``not(a)``         | ``~a``                   | Bitwise not   |
        +--------------------+--------------------------+---------------+
        | ``neg(a)``         | ``-a``                   | Negation      |
        +--------------------+--------------------------+---------------+



    Binary Operators:

        +-------------------+-------------------+-------------------------------+
        | Operator          | Implementation    | Description                   |
        +===================+===================+===============================+
        | ``add(a, b)``     | ``a + b``         | Addition                      |
        +-------------------+-------------------+-------------------------------+
        | ``sub(a, b)``     | ``a - b``         | Subtraction                   |
        +-------------------+-------------------+-------------------------------+
        | ``div(a, b)``     | ``a / b``         | Division                      |
        +-------------------+-------------------+-------------------------------+
        | ``mod(a, b)``     | ``a % b``         | Modulus                       |
        +-------------------+-------------------+-------------------------------+
        | ``mul(a, b)``     | ``a * b``         | Multiplication                |
        +-------------------+-------------------+-------------------------------+
        | ``and(a, b)``     | ``a & b``         | Bitwise and                   |
        +-------------------+-------------------+-------------------------------+
        | ``or(a, b)``      | ``a | b``         | Bitwise or                    |
        +-------------------+-------------------+-------------------------------+
        | ``xor(a, b)``     | ``a ^ b``         | Bitwise exclusive or          |
        +-------------------+-------------------+-------------------------------+
        | ``lshift(a, b)``  | ``a << b``        | Bitwise left shift            |
        +-------------------+-------------------+-------------------------------+
        | ``rshift(a, b)``  | ``a >> b``        | Bitwise signed right shift    |
        +-------------------+-------------------+-------------------------------+
        | ``zrshfit(a, b)`` | ``a >>> b``       | Bitwise unsigned right shift  |
        +-------------------+-------------------+-------------------------------+



    Built-in Comparators:

        +---------------+-------------------+---------------------------+
        | Operator      | Implementation    | Description               |
        +===============+===================+===========================+
        | ``eq(a, b)``  | ``a == b``        | Equals                    |
        +---------------+-------------------+---------------------------+
        | ``ne(a, b)``  | ``a != b``        | Not equals                |
        +---------------+-------------------+---------------------------+
        | ``gt(a, b)``  | ``a > b``         | Greater than              |
        +---------------+-------------------+---------------------------+
        | ``ge(a, b)``  | ``a >= b``        | Greater than or equal to  |
        +---------------+-------------------+---------------------------+
        | ``lt(a, b)``  | ``a < b``         | Less than                 |
        +---------------+-------------------+---------------------------+
        | ``le(a, b)``  | ``a <= b``        | Less than or equal to     |
        +---------------+-------------------+---------------------------+



    Extended Comparators (uses :mochiref:`compare`):

        +---------------+---------------------------+---------------------------+
        | Operator      | Implementation            | Description               |
        +===============+===========================+===========================+
        | ``ceq(a, b)`` | ``compare(a, b) == 0``    | Equals                    |
        +---------------+---------------------------+---------------------------+
        | ``cne(a, b)`` | ``compare(a, b) != 0``    | Not equals                |
        +---------------+---------------------------+---------------------------+
        | ``cgt(a, b)`` | ``compare(a, b) == 1``    | Greater than              |
        +---------------+---------------------------+---------------------------+
        | ``cge(a, b)`` | ``compare(a, b) != -1``   | Greater than or equal to  |
        +---------------+---------------------------+---------------------------+
        | ``clt(a, b)`` | ``compare(a, b) == -1``   | Less than                 |
        +---------------+---------------------------+---------------------------+
        | ``cle(a, b)`` | ``compare(a, b) != 1``    | Less than or equal to     |
        +---------------+---------------------------+---------------------------+



    Binary Logical Operators:

        +-----------------------+-------------------+---------------------------+
        | Operator              | Implementation    | Description               |
        +=======================+===================+===========================+
        | ``logand(a, b)``      | ``a && b``        | Logical and               |
        +-----------------------+-------------------+---------------------------+
        | ``logor(a, b)``       | ``a || b``        | Logical or                |
        +-----------------------+-------------------+---------------------------+
        | ``contains(a, b)``    | ``b in a``        | Has property (note order) |
        +-----------------------+-------------------+---------------------------+


:mochidef:`parseQueryString(encodedString[, useArrays=false])`:

    Parse a name=value pair URL query string into an object with a property
    for each pair. e.g.::

        var args = parseQueryString("foo=value%20one&bar=two");
        assert( args.foo == "value one" && args.bar == "two" );
    
    If you expect that the query string will reuse the
    same name, then give ``true`` as a second argument, which will
    use arrays to store the values. e.g.::

        var args = parseQueryString("foo=one&foo=two", true);
        assert( args.foo[0] == "one" && args.foo[1] == "two" );


:mochidef:`partial(func, arg[, ...])`:

    Return a partially applied function, e.g.::

        addNumbers = function (a, b) {
            return a + b;
        }

        addOne = partial(addNumbers, 1);

        assert(addOne(2) == 3);

    :mochiref:`partial` is a special form of :mochiref:`bind` that does not
    alter the bound ``self`` (if any). It is equivalent to calling::

        bind(func, undefined, arg[, ...]);

    See the documentation for :mochiref:`bind` for more details about
    this facility.
    
    This could be used to implement, but is NOT currying.
 

:mochidef:`queryString(names, values)`:

    Creates a URL query string from a pair of ``Array``-like objects
    representing ``names`` and ``values``. Each name=value pair will
    be URL encoded by :mochiref:`urlEncode`. name=value pairs with a
    value of ``undefined`` or ``null`` will be skipped. e.g.::

        var keys = ["foo", "bar"];
        var values = ["value one", "two"];
        assert( queryString(keys, values) == "foo=value%20one&bar=two" );

    Alternate form 1:
        :mochiref:`queryString(domElement)`

    If :mochiref:`MochiKit.DOM` is loaded, one argument is given, and that
    argument is either a string or has a ``nodeType`` property greater than
    zero, then ``names`` and ``values`` will be the result of
    :mochiref:`MochiKit.DOM.formContents(domElement)`.
    
    Alternate form 2:
        :mochiref:`queryString({name: value, ...})`

    Note that when using the alternate form, the order of the name=value
    pairs in the resultant query string is dependent on how the particular
    JavaScript implementation handles ``for (..in..)`` property enumeration.
    
    When using the second alternate form, name=value pairs with
    ``typeof(value) == "function"`` are ignored. This is a workaround for the
    case where a poorly designed library has modified ``Object.prototype``
    and inserted "convenience functions".


:mochidef:`registerComparator(name, check, comparator[, override])`:

    Register a comparator for use with :mochiref:`compare`.

    ``name``:
        unique identifier describing the comparator.

    ``check``:
        ``function(a, b)`` that returns ``true`` if ``a`` and ``b``
        can be compared with ``comparator``.

    ``comparator``:
        ``function(a, b)`` that returns:

        +-------+-----------+
        | Value | Condition |
        +-------+-----------+
        | 0     | a == b    |
        +-------+-----------+
        | 1     | a > b     |
        +-------+-----------+
        | -1    | a < b     |
        +-------+-----------+

        ``comparator`` is guaranteed to only be called if ``check(a, b)``
        returns a ``true`` value.

    ``override``:
        if ``true``, then this will be made the highest precedence comparator.
        Otherwise, the lowest.


:mochidef:`registerJSON(name, check, simplifier[, override])`:

    Register a simplifier function for use with :mochiref:`serializeJSON`.

    ``name``:
        unique identifier describing the serialization.

    ``check``:
        ``function(obj)`` that returns ``true`` if ``obj`` can
        can be simplified for serialization by ``simplifier``.

    ``simplifier``:
        ``function(obj)`` that returns a simpler object that
        can be further serialized by :mochiref:`serializeJSON`. For example,
        you could simplify ``Date``-like objects to ISO 8601 timestamp
        strings with the following simplifier::

            var simplifyDateAsISO = function (obj) {
                return toISOTimestamp(obj, true);
            };
            registerJSON("DateLike", isDateLike, simplifyDateAsISO);
        
        ``simplifier`` is guaranteed to only be called if ``check(obj)``
        returns a ``true`` value.

    ``override``:
        if ``true``, then this will be made the highest precedence comparator.
        Otherwise, the lowest.


:mochidef:`registerRepr(name, check, wrap[, override])`:

    Register a programmer representation function.
    :mochiref:`repr` functions should take one argument and 
    return a string representation of it
    suitable for developers, primarily used when debugging.

    If ``override`` is given, it is used as the highest priority
    repr, otherwise it will be used as the lowest.


:mochidef:`repr(obj)`:

    Return a programmer representation for ``obj``. See the
    `Programmer Representation`_ overview for more information about this
    function.


:mochidef:`reverseKeyComparator(key)`:

    A comparator factory that compares ``a[key]`` with ``b[key]`` in reverse.
    e.g.::

        var lst = ["a", "bbb", "cc"];
        lst.sort(reverseKeyComparator("length"));
        assert(lst.toString() == "bbb,cc,a");


:mochidef:`serializeJSON(anObject)`:

    Serialize ``anObject`` in the JSON [1]_ format, see `JSON Serialization`_
    for the coercion rules. For unserializable objects (functions that do
    not have an adapter, ``__json__`` method, or ``json`` method), this will
    return ``undefined``.

    For those familiar with Python, JSON is similar in scope to pickle, but
    it can not handle recursive object graphs.


:mochidef:`setdefault(self, obj[, ...])`:

    Mutate ``self`` by adding all properties from other object(s)
    that it does not already have set.
    
    If ``self`` is ``null``, a new ``Object`` instance will be created
    and returned.

    This mutates *and returns* ``self``, be warned.


:mochidef:`typeMatcher(typ[, ...])`:

    Given a set of types (as string arguments),
    returns a ``function(obj[, ...])`` that will return ``true`` if the
    types of the given arguments are all members of that set.


:mochidef:`update(self, obj[, ...])`:

    Mutate ``self`` by replacing its key:value pairs with those
    from other object(s). Key:value pairs from later objects will
    overwrite those from earlier objects.
    
    If ``self`` is ``null``, a new ``Object`` instance will be created
    and returned.

    This mutates *and returns* ``self``, be warned.

    A version of this function that creates a new object is available
    as :mochiref:`merge(a, b[, ...])`


:mochidef:`updatetree(self, obj[, ...])`:

    Mutate ``self`` by replacing its key:value pairs with those
    from other object(s). If a given key has an object value in
    both ``self`` and ``obj``, then this function will be called
    recursively, updating instead of replacing that object.

    If ``self`` is ``null``, a new ``Object`` instance will be created
    and returned.

    This mutates *and returns* ``self``, be warned.
    

:mochidef:`urlEncode(unencoded)`:

    Converts ``unencoded`` into a URL-encoded string. In this
    implementation, spaces are converted to %20 instead of "+". e.g.::
 
        assert( URLencode("1+2=2") == "1%2B2%3D2");


:mochidef:`xfilter(fn, obj[, ...])`:

    Returns a new ``Array`` composed of the arguments where
    ``fn(obj)`` returns a true value.

    If ``fn`` is ``null``, ``operator.truth`` will be used.


:mochidef:`xmap(fn, obj[, ...)`:

    Return a new ``Array`` composed of ``fn(obj)`` for every ``obj``
    given as an argument.

    If ``fn`` is ``null``, ``operator.identity`` is used.


See Also
========

.. [1] JSON, JavaScript Object Notation: http://json.org/
.. [2] Python's itertools
       module: http://docs.python.org/lib/module-itertools.html

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
