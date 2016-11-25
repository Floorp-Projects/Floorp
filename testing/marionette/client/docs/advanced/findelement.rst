Finding Elements
================
.. py:currentmodule:: marionette

One of the most common and yet often most difficult tasks in Marionette is
finding a DOM element on a webpage or in the chrome UI. Marionette provides
several different search strategies to use when finding elements. All search
strategies work with both :func:`~Marionette.find_element` and
:func:`~Marionette.find_elements`, though some strategies are not implemented
in chrome scope.

In the event that more than one element is matched by the query,
:func:`~Marionette.find_element` will only return the first element found. In
the event that no elements are matched by the query,
:func:`~Marionette.find_element` will raise `NoSuchElementException` while
:func:`~Marionette.find_elements` will return an empty list.

Search Strategies
-----------------

Search strategies are defined in the :class:`By` class::

    from marionette import By
    print(By.ID)

The strategies are:

* `id` - The easiest way to find an element is to refer to its id directly::

        container = client.find_element(By.ID, 'container')

* `class name` - To find elements belonging to a certain class, use `class name`::

        buttons = client.find_elements(By.CLASS_NAME, 'button')

* `css selector` - It's also possible to find elements using a `css selector`_::

        container_buttons = client.find_elements(By.CSS_SELECTOR, '#container .buttons')

* `name` - Find elements by their name attribute (not implemented in chrome
  scope)::

        form = client.find_element(By.NAME, 'signup')

* `tag name` - To find all the elements with a given tag, use `tag name`::

        paragraphs = client.find_elements(By.TAG_NAME, 'p')

* `link text` - A convenience strategy for finding link elements by their
  innerHTML (not implemented in chrome scope)::

        link = client.find_element(By.LINK_TEXT, 'Click me!')

* `partial link text` - Same as `link text` except substrings of the innerHTML
  are matched (not implemented in chrome scope)::

        link = client.find_element(By.PARTIAL_LINK_TEXT, 'Clic')

* `xpath` - Find elements using an xpath_ query::

        elem = client.find_element(By.XPATH, './/*[@id="foobar"')

.. _css selector: https://developer.mozilla.org/en-US/docs/Web/Guide/CSS/Getting_Started/Selectors
.. _xpath: https://developer.mozilla.org/en-US/docs/Web/XPath



Chaining Searches
-----------------

In addition to the methods on the Marionette object, HTMLElement objects also
provide :func:`~HTMLElement.find_element` and :func:`~HTMLElement.find_elements`
methods. The difference is that only child nodes of the element will be searched.
Consider the following html snippet::

    <div id="content">
        <span id="main"></span>
    </div>
    <div id="footer"></div>

Doing the following will work::

    client.find_element(By.ID, 'container').find_element(By.ID, 'main')

But this will raise a `NoSuchElementException`::

    client.find_element(By.ID, 'container').find_element(By.ID, 'footer')


Finding Anonymous Nodes
-----------------------

When working in chrome scope, for example manipulating the Firefox user
interface, you may run into something called an anonymous node.

Firefox uses a markup language called XUL_ for its interface. XUL is similar
to HTML in that it has a DOM and tags that render controls on the display. One
ability of XUL is to create re-useable widgets that are made up out of several
smaller XUL elements. These widgets can be bound to the DOM using something
called the `XML binding language (XBL)`_.

The end result is that the DOM sees the widget as a single entity. It doesn't
know anything about how that widget is made up. All of the smaller XUL elements
that make up the widget are called `anonymous content`_. It is not possible to
query such elements using traditional DOM methods like `getElementById`.

Marionette provides two special strategies used for finding anonymous content.
Unlike normal elements, anonymous nodes can only be seen by their parent. So
it's necessary to first find the parent element and then search for the
anonymous children from there.

* `anon` - Finds all anonymous children of the element, there is no search term
  so `None` must be passed in::

    anon_children = client.find_element('id', 'parent').find_elements('anon', None)

* `anon attribute` - Find an anonymous child based on an attribute. An
  unofficial convention is for anonymous nodes to have an
  `anonid` attribute::

    anon_child = client.find_element('id', 'parent').find_element('anon attribute', {'anonid': 'container'})


.. _XUL: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL
.. _XML binding language (XBL): https://developer.mozilla.org/en-US/docs/XBL
.. _anonymous content: https://developer.mozilla.org/en-US/docs/XBL/XBL_1.0_Reference/Anonymous_Content
