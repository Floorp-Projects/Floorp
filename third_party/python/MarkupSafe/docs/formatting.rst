.. currentmodule:: markupsafe

String Formatting
=================

The :class:`Markup` class can be used as a format string. Objects
formatted into a markup string will be escaped first.


Format Method
-------------

The ``format`` method extends the standard :meth:`str.format` behavior
to use an ``__html_format__`` method.

#.  If an object has an ``__html_format__`` method, it is called as a
    replacement for the ``__format__`` method. It is passed a format
    specifier if it's given. The method must return a string or
    :class:`Markup` instance.

#.  If an object has an ``__html__`` method, it is called. If a format
    specifier was passed and the class defined ``__html__`` but not
    ``__html_format__``, a ``ValueError`` is raised.

#.  Otherwise Python's default format behavior is used and the result
    is escaped.

For example, to implement a ``User`` that wraps its ``name`` in a
``span`` tag, and adds a link when using the ``'link'`` format
specifier:

.. code-block:: python

    class User(object):
        def __init__(self, id, name):
            self.id = id
            self.name = name

        def __html_format__(self, format_spec):
            if format_spec == 'link':
                return Markup(
                    '<a href="/user/{}">{}</a>'
                ).format(self.id, self.__html__())
            elif format_spec:
                raise ValueError('Invalid format spec')
            return self.__html__()

        def __html__(self):
            return Markup(
                '<span class="user">{0}</span>'
            ).format(self.name)


.. code-block:: pycon

    >>> user = User(3, '<script>')
    >>> escape(user)
    Markup('<span class="user">&lt;script&gt;</span>')
    >>> Markup('<p>User: {user:link}').format(user=user)
    Markup('<p>User: <a href="/user/3"><span class="user">&lt;script&gt;</span></a>

See Python's docs on :ref:`format string syntax <python:formatstrings>`.


printf-style Formatting
-----------------------

Besides escaping, there's no special behavior involved with percent
formatting.

.. code-block:: pycon

    >>> user = User(3, '<script>')
    >>> Markup('<a href="/user/%d">"%s</a>') % (user.id, user.name)
    Markup('<a href="/user/3">&lt;script&gt;</a>')

See Python's docs on :ref:`printf-style formatting <python:old-string-formatting>`.
