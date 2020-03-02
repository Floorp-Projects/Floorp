.. currentmodule:: markupsafe

MarkupSafe
==========

MarkupSafe escapes characters so text is safe to use in HTML and XML.
Characters that have special meanings are replaced so that they display
as the actual characters. This mitigates injection attacks, meaning
untrusted user input can safely be displayed on a page.

The :func:`escape` function escapes text and returns a :class:`Markup`
object. The object won't be escaped anymore, but any text that is used
with it will be, ensuring that the result remains safe to use in HTML.

>>> from markupsafe import escape
>>> hello = escape('<em>Hello</em>')
>>> hello
Markup('&lt;em&gt;Hello&lt;/em&gt;')
>>> escape(hello)
Markup('&lt;em&gt;Hello&lt;/em&gt;')
>>> hello + ' <strong>World</strong>'
Markup('&lt;em&gt;Hello&lt;/em&gt; &lt;strong&gt;World&lt;/strong&gt;')

.. note::

    The docs assume you're using Python 3. The terms "text" and "string"
    refer to the :class:`str` class. In Python 2, this would be the
    ``unicode`` class instead.


Installing
----------

Install and update using `pip`_:

.. code-block:: text

    pip install -U MarkupSafe

.. _pip: https://pip.pypa.io/en/stable/quickstart/


Table of Contents
-----------------

.. toctree::
    :maxdepth: 2

    escaping
    html
    formatting
    license
    changes
