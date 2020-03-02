.. currentmodule:: markupsafe

HTML Representations
====================

In many frameworks, if a class implements an ``__html__`` method it
will be used to get the object's representation in HTML. MarkupSafe's
:func:`escape` function and :class:`Markup` class understand and
implement this method. If an object has an ``__html__`` method it will
be called rather than converting the object to a string, and the result
will be assumed safe and not escaped.

For example, an ``Image`` class might automatically generate an
``<img>`` tag:

.. code-block:: python

    class Image:
        def __init__(self, url):
            self.url = url

        def __html__(self):
            return '<img src="%s">' % self.url

.. code-block:: pycon

    >>> img = Image('/static/logo.png')
    >>> Markup(img)
    Markup('<img src="/static/logo.png">')

Since this bypasses escaping, you need to be careful about using
user-provided data in the output. For example, a user's display name
should still be escaped:

.. code-block:: python

    class User:
        def __init__(self, id, name):
            self.id = id
            self.name = name

        def __html__(self):
            return '<a href="/user/{}">{}</a>'.format(
                self.id, escape(self.name)
            )

.. code-block:: pycon

    >>> user = User(3, '<script>')
    >>> escape(user)
    Markup('<a href="/users/3">&lt;script&gt;</a>')
