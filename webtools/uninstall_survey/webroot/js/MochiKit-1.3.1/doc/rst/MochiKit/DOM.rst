.. title:: MochiKit.DOM - painless DOM manipulation API

Name
====

MochiKit.DOM - painless DOM manipulation API


Synopsis
========

::

    var rows = [
        ["dataA1", "dataA2", "dataA3"],
        ["dataB1", "dataB2", "dataB3"]
    ];
    row_display = function (row) {
        return TR(null, map(partial(TD, null), row));
    }
    var newTable = TABLE({'class': 'prettytable'},
        THEAD(null,
            row_display(["head1", "head2", "head3"])),
        TFOOT(null,
            row_display(["foot1", "foot2", "foot3"])),
        TBODY(null,
            map(row_display, rows)));
    // put that in your document.createElement and smoke it!
    swapDOM(oldTable, newTable);


Description
===========

As you probably know, the DOM APIs are some of the most painful Java-inspired
APIs you'll run across from a highly dynamic language. Don't worry about that
though, because they provide a reasonable basis to build something that
sucks a lot less.

MochiKit.DOM takes much of its inspiration from Nevow's [1]_ stan [2]_.
This means you choose a tag, give it some attributes, then stuff it full
of *whatever objects you want*. MochiKit.DOM isn't stupid, it knows that
a string should be a text node, and that you want functions to be called,
and that ``Array``-like objects should be expanded, and stupid ``null`` values
should be skipped.

Hell, it will let you return strings from functions, and use iterators from
:mochiref:`MochiKit.Iter`. If that's not enough, just teach it new tricks with
:mochiref:`registerDOMConverter`. If you have never used an API like this for
creating DOM elements, you've been wasting your damn time. Get with it!


Dependencies
============

- :mochiref:`MochiKit.Base`
- :mochiref:`MochiKit.Iter`


Overview
========

DOM Coercion Rules
------------------

In order of precedence, :mochiref:`createDOM` coerces given arguments to DOM
nodes using the following rules:

1.  Functions are called with a ``this`` of the parent
    node and their return value is subject to the
    following rules (even this one).
2.  ``undefined`` and ``null`` are ignored.
3.  Iterables (see :mochiref:`MochiKit.Iter`) are flattened
    (as if they were passed in-line as nodes) and each
    return value is subject to all of these rules.
4.  Values that look like DOM nodes (objects with a
    ``.nodeType > 0``) are ``.appendChild``'ed to the created
    DOM fragment.
5.  Strings are wrapped up with ``document.createTextNode``
6.  Objects that are not strings are run through the ``domConverters``
    :mochiref:`MochiKit.Base.AdapterRegistry`
    (see :mochiref:`registerDOMConverter`).
    The value returned by the adapter is subject to these same rules (e.g.
    adapters are allowed to return a string, which will be coerced into a
    text node).
7.  If no adapter is available, ``.toString()`` is used to create a text node.


Creating DOM Element Trees
--------------------------

:mochiref:`createDOM` provides you with an excellent facility for creating DOM trees
that is easy on the wrists. One of the best ways to understand how to use
it is to take a look at an example::

    var rows = [
        ["dataA1", "dataA2", "dataA3"],
        ["dataB1", "dataB2", "dataB3"]
    ];
    row_display = function (row) {
        return TR(null, map(partial(TD, null), row));
    }
    var newTable = TABLE({'class': 'prettytable'},
        THEAD(null,
            row_display(["head1", "head2", "head3"])),
        TFOOT(null,
            row_display(["foot1", "foot2", "foot3"])),
        TBODY(null,
            map(row_display, rows)));
        

This will create a table with the following visual layout (if it
were inserted into the document DOM):

    +--------+--------+--------+
    | head1  | head2  | head3  |
    +========+========+========+
    | dataA1 | dataA2 | dataA3 |
    +--------+--------+--------+
    | dataB1 | dataB2 | dataB3 |
    +--------+--------+--------+
    | foot1  | foot2  | foot3  |
    +--------+--------+--------+

Corresponding to the following HTML::

    <table class="prettytable">
        <thead>
            <tr>
                <td>head1</td>
                <td>head2</td>
                <td>head3</td>
            </tr>
        </thead>
        <tfoot>
            <tr>
                <td>foot1</td>
                <td>foot2</td>
                <td>foot3</td>
            </tr>
        </tfoot>
        <tbody>
            <tr>
                <td>dataA1</td>
                <td>dataA2</td>
                <td>dataA3</td>
            </tr>
            <tr>
                <td>dataB1</td>
                <td>dataB2</td>
                <td>dataB3</td>
            </tr>
        </tbody>
    </table>


DOM Context
-----------

In order to prevent having to pass a ``window`` and/or ``document``
variable to every MochiKit.DOM function (e.g. when working with a
child window), MochiKit.DOM maintains a context variable for each
of them. They are managed with the :mochiref:`withWindow` and
:mochiref:`withDocument` functions, and can be acquired with
:mochiref:`currentWindow()` and :mochiref:`currentDocument()`

For example, if you are creating DOM nodes in a child window, you
could do something like this::

    withWindow(child, function () {
        var doc = currentDocument();
        appendChildNodes(doc.body, H1(null, "This is in the child!"));
    });

Note that :mochiref:`withWindow(win, ...)` also implies
:mochiref:`withDocument(win.document, ...)`.


Element Visibility
------------------

The :mochiref:`hideElement` and :mochiref:`showElement` functions are
provided as a convenience, but only work for elements that are
``display: block``. For a general solution to showing, hiding, and checking
the explicit visibility of elements, we recommend using a solution that
involves a little CSS. Here's an example::

    <style type="text/css">
        .invisible { display: none; }
    </style>

    <script type="text/javascript">
        function toggleVisible(elem) {
            toggleElementClass("invisible", elem); 
        }

        function makeVisible(elem) {
            removeElementClass(elem, "invisible");
        }

        function makeInvisible(elem) {
            addElementClass(elem, "invisible");
        }

        function isVisible(elem) {
            // you may also want to check for
            // getElement(elem).style.display == "none"
            return !hasElementClass(elem, "invisible");
        }; 
    </script>

MochiKit doesn't ship with such a solution, because there is no reliable and
portable method for adding CSS rules on the fly with JavaScript.


API Reference
=============

Functions
---------

:mochidef:`$(id[, ...])`:

    An alias for :mochiref:`getElement(id[, ...])`


:mochidef:`addElementClass(element, className)`:

    Ensure that the given ``element`` has ``className`` set as part of its
    class attribute. This will not disturb other class names.
    ``element`` is looked up with :mochiref:`getElement`, so string identifiers
    are also acceptable.


:mochidef:`addLoadEvent(func)`:

    Note that :mochiref:`addLoadEvent` can not be used in combination with
    :mochiref:`MochiKit.Signal` if the ``onload`` event is connected.
    Once an event is connected with :mochiref:`MochiKit.Signal`, no other APIs
    may be used for that same event.

    This will stack ``window.onload`` functions on top of each other.
    Each function added will be called after ``onload`` in the
    order that they were added.


:mochidef:`addToCallStack(target, path, func[, once])`:

    Note that :mochiref:`addToCallStack` is not compatible with 
    :mochiref:`MochiKit.Signal`. Once an event is connected with
    :mochiref:`MochiKit.Signal`, no other APIs may be used for that same event.

    Set the property ``path`` of ``target`` to a function that calls the
    existing function at that property (if any), then calls ``func``.

    If ``target[path]()`` returns exactly ``false``, then ``func`` will
    not be called.

    If ``once`` is ``true``, then ``target[path]`` is set to ``null`` after
    the function call stack has completed.

    If called several times for the same ``target[path]``, it will create
    a stack of functions (instead of just a pair).


:mochidef:`appendChildNodes(node[, childNode[, ...]])`:

    Append children to a DOM element using the `DOM Coercion Rules`_.

    ``node``:
        A reference to the DOM element to add children to
        (if a string is given, :mochiref:`getElement(node)`
        will be used to locate the node)

    ``childNode``...:
        All additional arguments, if any, will be coerced into DOM
        nodes that are appended as children using the
        `DOM Coercion Rules`_.

    *returns*:
        The given DOM element


:mochidef:`computedStyle(htmlElement, cssProperty, mozillaEquivalentCSS)`:

    Looks up a CSS property for the given element. The element can be
    specified as either a string with the element's ID or the element
    object itself.


:mochidef:`createDOM(name[, attrs[, node[, ...]]])`:

    Create a DOM fragment in a really convenient manner, much like
    Nevow`s [1]_ stan [2]_.

    Partially applied versions of this function for common tags are
    available as aliases:

    - ``A``
    - ``BUTTON``
    - ``BR``
    - ``CANVAS``
    - ``DIV``
    - ``FIELDSET``
    - ``FORM``
    - ``H1``
    - ``H2``
    - ``H3``
    - ``HR``
    - ``IMG``
    - ``INPUT``
    - ``LABEL``
    - ``LEGEND``
    - ``LI``
    - ``OL``
    - ``OPTGROUP``
    - ``OPTION``
    - ``P``
    - ``PRE``
    - ``SELECT``
    - ``SPAN``
    - ``STRONG``
    - ``TABLE``
    - ``TBODY``
    - ``TD``
    - ``TEXTAREA``
    - ``TFOOT``
    - ``TH``
    - ``THEAD``
    - ``TR``
    - ``TT``
    - ``UL``

    See `Creating DOM Element Trees`_ for a comprehensive example.

    ``name``:
        The kind of fragment to create (e.g. 'span'), such as you would
        pass to ``document.createElement``.

    ``attrs``:
        An object whose properties will be used as the attributes
        (e.g. ``{'style': 'display:block'}``), or ``null`` if no
        attributes need to be set.

        See :mochiref:`updateNodeAttributes` for more information.

        For convenience, if ``attrs`` is a string, ``null`` is used
        and the string will be considered the first ``node``.

    ``node``...:
        All additional arguments, if any, will be coerced into DOM
        nodes that are appended as children using the
        `DOM Coercion Rules`_.

    *returns*:
        A DOM element


:mochidef:`createDOMFunc(tag[, attrs[, node[, ...]]])`:
    
    Convenience function to create a partially applied createDOM
    function. You'd want to use this if you add additional convenience
    functions for creating tags, or if you find yourself creating
    a lot of tags with a bunch of the same attributes or contents.

    See :mochiref:`createDOM` for more detailed descriptions of the arguments.

    ``tag``:
        The name of the tag

    ``attrs``:
        Optionally specify the attributes to apply

    ``node``...:
        Optionally specify any children nodes it should have

    *returns*:
        function that takes additional arguments and calls
        :mochiref:`createDOM`


:mochidef:`currentDocument()`:

    Return the current ``document`` `DOM Context`_. This will always
    be the same as the global ``document`` unless :mochiref:`withDocument` or
    :mochiref:`withWindow` is currently executing.


:mochidef:`currentWindow()`:

    Return the current ``window`` `DOM Context`_. This will always
    be the same as the global ``window`` unless :mochiref:`withWindow` is 
    currently executing.


:mochidef:`elementDimensions(element)`:

    Return the absolute pixel width and height of ``element`` as an object with
    ``w`` and ``h`` properties, or ``undefined`` if ``element`` is not in the
    document. ``element`` may be specified as a string to be looked up with
    :mochiref:`getElement`, a DOM element, or trivially as an object with
    ``w`` and/or ``h`` properties.


:mochidef:`elementPosition(element[, relativeTo={x: 0, y: 0}])`:

    Return the absolute pixel position of ``element`` in the document as an
    object with ``x`` and ``y`` properties, or ``undefined`` if ``element``
    is not in the document. ``element`` may be specified as a string to
    be looked up with :mochiref:`getElement`, a DOM element, or trivially
    as an object with ``x`` and/or ``y`` properties.

    If ``relativeTo`` is given, then its coordinates are subtracted from
    the absolute position of ``element``, e.g.::
        
        var elemPos = elementPosition(elem);
        var anotherElemPos = elementPosition(anotherElem);
        var relPos = elementPosition(elem, anotherElem);
        assert( relPos.x == (elemPos.x - anotherElemPos.x) );
        assert( relPos.y == (elemPos.y - anotherElemPos.y) );

    ``relativeTo`` may be specified as a string to be looked up with
    :mochiref:`getElement`, a DOM element, or trivially as an object
    with ``x`` and/or ``y`` properties.


:mochidef:`emitHTML(dom[, lst])`:

    Convert a DOM tree to an ``Array`` of HTML string fragments

    You probably want to use :mochiref:`toHTML` instead.


:mochidef:`escapeHTML(s)`:

    Make a string safe for HTML, converting the usual suspects (lt,
    gt, quot, apos, amp)


:mochidef:`focusOnLoad(element)`:

    Add an onload event to focus the given element
       

:mochidef:`formContents(elem)`:

    Search the DOM tree, starting at ``elem``, for any elements with a
    ``name`` and ``value`` attribute. Return a 2-element ``Array`` of 
    ``names`` and ``values`` suitable for use with
    :mochiref:`MochiKit.Base.queryString`.


:mochidef:`getElement(id[, ...])`:

    A small quick little function to encapsulate the ``getElementById``
    method. It includes a check to ensure we can use that method.

    If the id isn't a string, it will be returned as-is.

    Also available as :mochiref:`$(...)` for convenience and compatibility with
    other JavaScript frameworks.

    If multiple arguments are given, an ``Array`` will be returned.


:mochidef:`getElementsByTagAndClassName(tagName, className, parent=document)`:

    Returns an array of elements in ``parent`` that match the tag name
    and class name provided. If ``parent`` is a string, it will be looked
    up with :mochiref:`getElement`.
    
    If ``tagName`` is ``null`` or ``"*"``, all elements will be searched 
    for the matching class.
    
    If ``className`` is ``null``, all elements matching the provided tag are
    returned.


:mochidef:`getNodeAttribute(node, attr)`:

    Get the value of the given attribute for a DOM element without
    ever raising an exception (will return ``null`` on exception).
    
    ``node``:
        A reference to the DOM element to update (if a string is given,
        :mochiref:`getElement(node)` will be used to locate the node)

    ``attr``:
        The name of the attribute

        Note that it will do the right thing for IE, so don't do
        the ``class`` -> ``className`` hack yourself.

    *returns*:
        The attribute's value, or ``null``


:mochidef:`getViewportDimensions()`:

    Return the pixel width and height of the viewport as an object with ``w``
    and ``h`` properties. ``element`` is looked up with
    :mochiref:`getElement`, so string identifiers are also acceptable.


:mochidef:`hasElementClass(element, className[, ...])`:

    Return ``true`` if ``className`` is found on the ``element``.
    ``element`` is looked up with :mochiref:`getElement`, so string identifiers
    are also acceptable.


:mochidef:`hideElement(element, ...)`:

    Partial form of :mochiref:`setDisplayForElement`, specifically::

        partial(setDisplayForElement, "none")

    For information about the caveats of using a ``style.display`` based
    show/hide mechanism, and a CSS based alternative, see
    `Element Visibility`_.


:mochidef:`registerDOMConverter(name, check, wrap[, override])`:

    Register an adapter to convert objects that match ``check(obj, ctx)``
    to a DOM element, or something that can be converted to a DOM
    element (i.e. number, bool, string, function, iterable).


:mochidef:`removeElement(node)`:

    Remove and return ``node`` from a DOM tree. This is technically
    just a convenience for :mochiref:`swapDOM(node, null)`.

    ``node``:
        the DOM element (or string id of one) to be removed

    *returns*
        The removed element


:mochidef:`removeElementClass(element, className)`:

    Ensure that the given ``element`` does not have ``className`` set as part
    of its class attribute. This will not disturb other class names.
    ``element`` is looked up with :mochiref:`getElement`, so string identifiers
    are also acceptable.


:mochidef:`replaceChildNodes(node[, childNode[, ...]])`:

    Remove all children from the given DOM element, then append any given
    childNodes to it (by calling :mochiref:`appendChildNodes`).

    ``node``:
        A reference to the DOM element to add children to
        (if a string is given, :mochiref:`getElement(node)`
        will be used to locate the node)

    ``childNode``...:
        All additional arguments, if any, will be coerced into DOM
        nodes that are appended as children using the
        `DOM Coercion Rules`_.

    *returns*:
        The given DOM element


:mochidef:`scrapeText(node[, asArray=false])`:

    Walk a DOM tree in-order and scrape all of the text out of it as a
    ``string``.

    If ``asArray`` is ``true``, then an ``Array`` will be returned with
    each individual text node. These two are equivalent::

        assert( scrapeText(node) == scrapeText(node, true).join("") );


:mochidef:`setDisplayForElement(display, element[, ...])`:

    Change the ``style.display`` for the given element(s). Usually
    used as the partial forms:

    - :mochiref:`showElement(element, ...)`
    - :mochiref:`hideElement(element, ...)`

    Elements are looked up with :mochiref:`getElement`, so string identifiers
    are acceptable.

    For information about the caveats of using a ``style.display`` based
    show/hide mechanism, and a CSS based alternative, see
    `Element Visibility`_.


:mochidef:`setElementClass(element, className)`:

    Set the entire class attribute of ``element`` to ``className``.
    ``element`` is looked up with :mochiref:`getElement`, so string identifiers
    are also acceptable.
        

:mochidef:`setElementDimensions(element, dimensions[, units='px'])`:

    Sets the dimensions of ``element`` in the document from an
    object with ``w`` and ``h`` properties.
    
    ``node``:
        A reference to the DOM element to update (if a string is given,
        :mochiref:`getElement(node)` will be used to locate the node)
        
    ``dimensions``:
        An object with ``w`` and ``h`` properties
        
    ``units``:
        Optionally set the units to use, default is ``px``


:mochidef:`setElementPosition(element, position[, units='px'])`:

    Sets the absolute position of ``element`` in the document from an
    object with ``x`` and ``y`` properties.
    
    ``node``:
        A reference to the DOM element to update (if a string is given,
        :mochiref:`getElement(node)` will be used to locate the node)
        
    ``position``:
        An object with ``x`` and ``y`` properties
        
    ``units``:
        Optionally set the units to use, default is ``px``


:mochidef:`setNodeAttribute(node, attr, value)`:

    Set the value of the given attribute for a DOM element without
    ever raising an exception (will return null on exception). If
    setting more than one attribute, you should use
    :mochiref:`updateNodeAttributes`.
    
    ``node``:
        A reference to the DOM element to update (if a string is given,
        :mochiref:`getElement(node)` will be used to locate the node)

    ``attr``:
        The name of the attribute

        Note that it will do the right thing for IE, so don't do
        the ``class`` -> ``className`` hack yourself.

    ``value``:
        The value of the attribute, may be an object to be merged
        (e.g. for setting style).

    *returns*:
        The given DOM element or ``null`` on failure


:mochidef:`setOpacity(element, opacity)`:

    Sets ``opacity`` for ``element``. Valid ``opacity`` values range from 0
    (invisible) to 1 (opaque). ``element`` is looked up with
    :mochiref:`getElement`, so string identifiers are also acceptable.


:mochidef:`showElement(element, ...)`:

    Partial form of :mochiref:`setDisplayForElement`, specifically::

        partial(setDisplayForElement, "block")

    For information about the caveats of using a ``style.display`` based
    show/hide mechanism, and a CSS based alternative, see
    `Element Visibility`_.


:mochidef:`swapDOM(dest, src)`:

    Replace ``dest`` in a DOM tree with ``src``, returning ``src``.

    ``dest``:
        a DOM element (or string id of one) to be replaced

    ``src``:
        the DOM element (or string id of one) to replace it with, or
        ``null`` if ``dest`` is to be removed (replaced with nothing).

    *returns*:
        a DOM element (``src``)


:mochidef:`swapElementClass(element, fromClass, toClass)`:

    If ``fromClass`` is set on ``element``, replace it with ``toClass``.
    This will not disturb other classes on that element.
    ``element`` is looked up with :mochiref:`getElement`, so string identifiers
    are also acceptable.


:mochidef:`toggleElementClass(className[, element[, ...]])`:

    Toggle the presence of a given ``className`` in the class attribute
    of all given elements. All elements will be looked up with
    :mochiref:`getElement`, so string identifiers are acceptable.


:mochidef:`toHTML(dom)`:

    Convert a DOM tree to a HTML string using :mochiref:`emitHTML`


:mochidef:`updateNodeAttributes(node, attrs)`:

    Update the attributes of a DOM element from a given object.
    
    ``node``:
        A reference to the DOM element to update (if a string is given,
        :mochiref:`getElement(node)` will be used to locate the node)

    ``attrs``:
        An object whose properties will be used to set the attributes
        (e.g. ``{'class': 'invisible'}``), or ``null`` if no
        attributes need to be set. If an object is given for the
        attribute value (e.g. ``{'style': {'display': 'block'}}``)
        then :mochiref:`MochiKit.Base.updatetree` will be used to set that
        attribute.

        Note that it will do the right thing for IE, so don't do
        the ``class`` -> ``className`` hack yourself, and it deals with
        setting "on..." event handlers correctly.

    *returns*:
        The given DOM element


:mochidef:`withWindow(win, func)`:

    Call ``func`` with the ``window`` `DOM Context`_ set to ``win`` and
    the ``document`` `DOM Context`_ set to ``win.document``. When
    ``func()`` returns or throws an error, the `DOM Context`_  will be
    restored to its previous state.
    
    The return value of ``func()`` is returned by this function.


:mochidef:`withDocument(doc, func)`:

    Call ``func`` with the ``doc`` `DOM Context`_ set to ``doc``.
    When ``func()`` returns or throws an error, the `DOM Context`_
    will be restored to its previous state.
    
    The return value of ``func()`` is returned by this function.


See Also
========

.. [1] Nevow, a web application construction kit for Python: http://nevow.com/
.. [2] nevow.stan is a domain specific language for Python 
       (read as "crazy getitem/call overloading abuse") that Donovan and I
       schemed up at PyCon 2003 at this super ninja Python/C++ programmer's
       (David Abrahams) hotel room. Donovan later inflicted this upon the
       masses in Nevow. Check out the Divmod project page for some
       examples: http://nevow.com/Nevow2004Tutorial.html


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
