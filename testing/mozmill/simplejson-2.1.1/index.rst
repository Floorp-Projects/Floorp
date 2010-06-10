:mod:`simplejson` --- JSON encoder and decoder
==============================================

.. module:: simplejson
   :synopsis: Encode and decode the JSON format.
.. moduleauthor:: Bob Ippolito <bob@redivi.com>
.. sectionauthor:: Bob Ippolito <bob@redivi.com>

JSON (JavaScript Object Notation) <http://json.org> is a subset of JavaScript
syntax (ECMA-262 3rd edition) used as a lightweight data interchange format.

:mod:`simplejson` exposes an API familiar to users of the standard library
:mod:`marshal` and :mod:`pickle` modules. It is the externally maintained
version of the :mod:`json` library contained in Python 2.6, but maintains
compatibility with Python 2.5 and (currently) has
significant performance advantages, even without using the optional C
extension for speedups.

Encoding basic Python object hierarchies::

    >>> import simplejson as json
    >>> json.dumps(['foo', {'bar': ('baz', None, 1.0, 2)}])
    '["foo", {"bar": ["baz", null, 1.0, 2]}]'
    >>> print json.dumps("\"foo\bar")
    "\"foo\bar"
    >>> print json.dumps(u'\u1234')
    "\u1234"
    >>> print json.dumps('\\')
    "\\"
    >>> print json.dumps({"c": 0, "b": 0, "a": 0}, sort_keys=True)
    {"a": 0, "b": 0, "c": 0}
    >>> from StringIO import StringIO
    >>> io = StringIO()
    >>> json.dump(['streaming API'], io)
    >>> io.getvalue()
    '["streaming API"]'

Compact encoding::

    >>> import simplejson as json
    >>> json.dumps([1,2,3,{'4': 5, '6': 7}], separators=(',',':'))
    '[1,2,3,{"4":5,"6":7}]'

Pretty printing::

    >>> import simplejson as json
    >>> s = json.dumps({'4': 5, '6': 7}, sort_keys=True, indent=4 * ' ')
    >>> print '\n'.join([l.rstrip() for l in  s.splitlines()])
    {
        "4": 5,
        "6": 7
    }

Decoding JSON::

    >>> import simplejson as json
    >>> obj = [u'foo', {u'bar': [u'baz', None, 1.0, 2]}]
    >>> json.loads('["foo", {"bar":["baz", null, 1.0, 2]}]') == obj
    True
    >>> json.loads('"\\"foo\\bar"') == u'"foo\x08ar'
    True
    >>> from StringIO import StringIO
    >>> io = StringIO('["streaming API"]')
    >>> json.load(io)[0] == 'streaming API'
    True

Using Decimal instead of float::

    >>> import simplejson as json
    >>> from decimal import Decimal
    >>> json.loads('1.1', use_decimal=True) == Decimal('1.1')
    True
    >>> json.dumps(Decimal('1.1'), use_decimal=True) == '1.1'
    True

Specializing JSON object decoding::

    >>> import simplejson as json
    >>> def as_complex(dct):
    ...     if '__complex__' in dct:
    ...         return complex(dct['real'], dct['imag'])
    ...     return dct
    ...
    >>> json.loads('{"__complex__": true, "real": 1, "imag": 2}',
    ...     object_hook=as_complex)
    (1+2j)
    >>> import decimal
    >>> json.loads('1.1', parse_float=decimal.Decimal) == decimal.Decimal('1.1')
    True

Specializing JSON object encoding::

    >>> import simplejson as json
    >>> def encode_complex(obj):
    ...     if isinstance(obj, complex):
    ...         return [obj.real, obj.imag]
    ...     raise TypeError(repr(o) + " is not JSON serializable")
    ...
    >>> json.dumps(2 + 1j, default=encode_complex)
    '[2.0, 1.0]'
    >>> json.JSONEncoder(default=encode_complex).encode(2 + 1j)
    '[2.0, 1.0]'
    >>> ''.join(json.JSONEncoder(default=encode_complex).iterencode(2 + 1j))
    '[2.0, 1.0]'


.. highlight:: none

Using :mod:`simplejson.tool` from the shell to validate and pretty-print::

    $ echo '{"json":"obj"}' | python -m simplejson.tool
    {
        "json": "obj"
    }
    $ echo '{ 1.2:3.4}' | python -m simplejson.tool
    Expecting property name: line 1 column 2 (char 2)

.. highlight:: python

.. note::

   The JSON produced by this module's default settings is a subset of
   YAML, so it may be used as a serializer for that as well.


Basic Usage
-----------

.. function:: dump(obj, fp[, skipkeys[, ensure_ascii[, check_circular[, allow_nan[, cls[, indent[, separators[, encoding[, default[, use_decimal[, **kw]]]]]]]]]]])

   Serialize *obj* as a JSON formatted stream to *fp* (a ``.write()``-supporting
   file-like object).

   If *skipkeys* is true (default: ``False``), then dict keys that are not
   of a basic type (:class:`str`, :class:`unicode`, :class:`int`, :class:`long`,
   :class:`float`, :class:`bool`, ``None``) will be skipped instead of raising a
   :exc:`TypeError`.

   If *ensure_ascii* is false (default: ``True``), then some chunks written
   to *fp* may be :class:`unicode` instances, subject to normal Python
   :class:`str` to :class:`unicode` coercion rules.  Unless ``fp.write()``
   explicitly understands :class:`unicode` (as in :func:`codecs.getwriter`) this
   is likely to cause an error. It's best to leave the default settings, because
   they are safe and it is highly optimized.

   If *check_circular* is false (default: ``True``), then the circular
   reference check for container types will be skipped and a circular reference
   will result in an :exc:`OverflowError` (or worse).

   If *allow_nan* is false (default: ``True``), then it will be a
   :exc:`ValueError` to serialize out of range :class:`float` values (``nan``,
   ``inf``, ``-inf``) in strict compliance of the JSON specification.
   If *allow_nan* is true, their JavaScript equivalents will be used
   (``NaN``, ``Infinity``, ``-Infinity``).

   If *indent* is a string, then JSON array elements and object members
   will be pretty-printed with a newline followed by that string repeated
   for each level of nesting. ``None`` (the default) selects the most compact
   representation without any newlines. For backwards compatibility with
   versions of simplejson earlier than 2.1.0, an integer is also accepted
   and is converted to a string with that many spaces.

   .. versionchanged:: 2.1.0
      Changed *indent* from an integer number of spaces to a string.

   If specified, *separators* should be an ``(item_separator, dict_separator)``
   tuple.  By default, ``(', ', ': ')`` are used.  To get the most compact JSON
   representation, you should specify ``(',', ':')`` to eliminate whitespace.

   *encoding* is the character encoding for str instances, default is
   ``'utf-8'``.

   *default(obj)* is a function that should return a serializable version of
   *obj* or raise :exc:`TypeError`.  The default simply raises :exc:`TypeError`.

   To use a custom :class:`JSONEncoder` subclass (e.g. one that overrides the
   :meth:`default` method to serialize additional types), specify it with the
   *cls* kwarg.
   
   If *use_decimal* is true (default: ``False``) then :class:`decimal.Decimal`
   will be natively serialized to JSON with full precision.
   
   .. versionchanged:: 2.1.0
      *use_decimal* is new in 2.1.0.

    .. note::

        JSON is not a framed protocol so unlike :mod:`pickle` or :mod:`marshal` it
        does not make sense to serialize more than one JSON document without some
        container protocol to delimit them.


.. function:: dumps(obj[, skipkeys[, ensure_ascii[, check_circular[, allow_nan[, cls[, indent[, separators[, encoding[, default[, use_decimal[, **kw]]]]]]]]]]])

   Serialize *obj* to a JSON formatted :class:`str`.

   If *ensure_ascii* is false, then the return value will be a
   :class:`unicode` instance.  The other arguments have the same meaning as in
   :func:`dump`. Note that the default *ensure_ascii* setting has much
   better performance.


.. function:: load(fp[, encoding[, cls[, object_hook[, parse_float[, parse_int[, parse_constant[, object_pairs_hook[, use_decimal[, **kw]]]]]]]]])

   Deserialize *fp* (a ``.read()``-supporting file-like object containing a JSON
   document) to a Python object.

   If the contents of *fp* are encoded with an ASCII based encoding other than
   UTF-8 (e.g. latin-1), then an appropriate *encoding* name must be specified.
   Encodings that are not ASCII based (such as UCS-2) are not allowed, and
   should be wrapped with ``codecs.getreader(fp)(encoding)``, or simply decoded
   to a :class:`unicode` object and passed to :func:`loads`. The default
   setting of ``'utf-8'`` is fastest and should be using whenever possible.

   If *fp.read()* returns :class:`str` then decoded JSON strings that contain
   only ASCII characters may be parsed as :class:`str` for performance and
   memory reasons. If your code expects only :class:`unicode` the appropriate
   solution is to wrap fp with a reader as demonstrated above.

   *object_hook* is an optional function that will be called with the result of
   any object literal decode (a :class:`dict`).  The return value of
   *object_hook* will be used instead of the :class:`dict`.  This feature can be used
   to implement custom decoders (e.g. JSON-RPC class hinting).

   *object_pairs_hook* is an optional function that will be called with the
   result of any object literal decode with an ordered list of pairs.  The
   return value of *object_pairs_hook* will be used instead of the
   :class:`dict`.  This feature can be used to implement custom decoders that
   rely on the order that the key and value pairs are decoded (for example,
   :class:`collections.OrderedDict` will remember the order of insertion). If
   *object_hook* is also defined, the *object_pairs_hook* takes priority.

   .. versionchanged:: 2.1.0
      Added support for *object_pairs_hook*.

   *parse_float*, if specified, will be called with the string of every JSON
   float to be decoded.  By default, this is equivalent to ``float(num_str)``.
   This can be used to use another datatype or parser for JSON floats
   (e.g. :class:`decimal.Decimal`).

   *parse_int*, if specified, will be called with the string of every JSON int
   to be decoded.  By default, this is equivalent to ``int(num_str)``.  This can
   be used to use another datatype or parser for JSON integers
   (e.g. :class:`float`).

   *parse_constant*, if specified, will be called with one of the following
   strings: ``'-Infinity'``, ``'Infinity'``, ``'NaN'``.  This can be used to
   raise an exception if invalid JSON numbers are encountered.

   If *use_decimal* is true (default: ``False``) then *parse_float* is set to
   :class:`decimal.Decimal`. This is a convenience for parity with the
   :func:`dump` parameter.
   
   .. versionchanged:: 2.1.0
      *use_decimal* is new in 2.1.0.

   To use a custom :class:`JSONDecoder` subclass, specify it with the ``cls``
   kwarg.  Additional keyword arguments will be passed to the constructor of the
   class.

    .. note::

        :func:`load` will read the rest of the file-like object as a string and
        then call :func:`loads`. It does not stop at the end of the first valid
        JSON document it finds and it will raise an error if there is anything
        other than whitespace after the document. Except for files containing
        only one JSON document, it is recommended to use :func:`loads`.


.. function:: loads(s[, encoding[, cls[, object_hook[, parse_float[, parse_int[, parse_constant[, object_pairs_hook[, use_decimal[, **kw]]]]]]]]])

   Deserialize *s* (a :class:`str` or :class:`unicode` instance containing a JSON
   document) to a Python object.

   If *s* is a :class:`str` instance and is encoded with an ASCII based encoding
   other than UTF-8 (e.g. latin-1), then an appropriate *encoding* name must be
   specified.  Encodings that are not ASCII based (such as UCS-2) are not
   allowed and should be decoded to :class:`unicode` first.

   If *s* is a :class:`str` then decoded JSON strings that contain
   only ASCII characters may be parsed as :class:`str` for performance and
   memory reasons. If your code expects only :class:`unicode` the appropriate
   solution is decode *s* to :class:`unicode` prior to calling loads.

   The other arguments have the same meaning as in :func:`load`.


Encoders and decoders
---------------------

.. class:: JSONDecoder([encoding[, object_hook[, parse_float[, parse_int[, parse_constant[, object_pairs_hook[, strict]]]]]]])

   Simple JSON decoder.

   Performs the following translations in decoding by default:

   +---------------+-------------------+
   | JSON          | Python            |
   +===============+===================+
   | object        | dict              |
   +---------------+-------------------+
   | array         | list              |
   +---------------+-------------------+
   | string        | unicode           |
   +---------------+-------------------+
   | number (int)  | int, long         |
   +---------------+-------------------+
   | number (real) | float             |
   +---------------+-------------------+
   | true          | True              |
   +---------------+-------------------+
   | false         | False             |
   +---------------+-------------------+
   | null          | None              |
   +---------------+-------------------+

   It also understands ``NaN``, ``Infinity``, and ``-Infinity`` as their
   corresponding ``float`` values, which is outside the JSON spec.

   *encoding* determines the encoding used to interpret any :class:`str` objects
   decoded by this instance (``'utf-8'`` by default).  It has no effect when decoding
   :class:`unicode` objects.

   Note that currently only encodings that are a superset of ASCII work, strings
   of other encodings should be passed in as :class:`unicode`.

   *object_hook* is an optional function that will be called with the result of
   every JSON object decoded and its return value will be used in place of the
   given :class:`dict`.  This can be used to provide custom deserializations
   (e.g. to support JSON-RPC class hinting).

   *object_pairs_hook* is an optional function that will be called with the
   result of any object literal decode with an ordered list of pairs.  The
   return value of *object_pairs_hook* will be used instead of the
   :class:`dict`.  This feature can be used to implement custom decoders that
   rely on the order that the key and value pairs are decoded (for example,
   :class:`collections.OrderedDict` will remember the order of insertion). If
   *object_hook* is also defined, the *object_pairs_hook* takes priority.

   .. versionchanged:: 2.1.0
      Added support for *object_pairs_hook*.

   *parse_float*, if specified, will be called with the string of every JSON
   float to be decoded.  By default, this is equivalent to ``float(num_str)``.
   This can be used to use another datatype or parser for JSON floats
   (e.g. :class:`decimal.Decimal`).

   *parse_int*, if specified, will be called with the string of every JSON int
   to be decoded.  By default, this is equivalent to ``int(num_str)``.  This can
   be used to use another datatype or parser for JSON integers
   (e.g. :class:`float`).

   *parse_constant*, if specified, will be called with one of the following
   strings: ``'-Infinity'``, ``'Infinity'``, ``'NaN'``.  This can be used to
   raise an exception if invalid JSON numbers are encountered.

   *strict* controls the parser's behavior when it encounters an invalid
   control character in a string. The default setting of ``True`` means that
   unescaped control characters are parse errors, if ``False`` then control
   characters will be allowed in strings.

   .. method:: decode(s)

      Return the Python representation of *s* (a :class:`str` or
      :class:`unicode` instance containing a JSON document)

      If *s* is a :class:`str` then decoded JSON strings that contain
      only ASCII characters may be parsed as :class:`str` for performance and
      memory reasons. If your code expects only :class:`unicode` the
      appropriate solution is decode *s* to :class:`unicode` prior to calling
      decode.

   .. method:: raw_decode(s)

      Decode a JSON document from *s* (a :class:`str` or :class:`unicode`
      beginning with a JSON document) and return a 2-tuple of the Python
      representation and the index in *s* where the document ended.

      This can be used to decode a JSON document from a string that may have
      extraneous data at the end.


.. class:: JSONEncoder([skipkeys[, ensure_ascii[, check_circular[, allow_nan[, sort_keys[, indent[, separators[, encoding[, default]]]]]]]]])

   Extensible JSON encoder for Python data structures.

   Supports the following objects and types by default:

   +-------------------+---------------+
   | Python            | JSON          |
   +===================+===============+
   | dict              | object        |
   +-------------------+---------------+
   | list, tuple       | array         |
   +-------------------+---------------+
   | str, unicode      | string        |
   +-------------------+---------------+
   | int, long, float  | number        |
   +-------------------+---------------+
   | True              | true          |
   +-------------------+---------------+
   | False             | false         |
   +-------------------+---------------+
   | None              | null          |
   +-------------------+---------------+

   To extend this to recognize other objects, subclass and implement a
   :meth:`default` method with another method that returns a serializable object
   for ``o`` if possible, otherwise it should call the superclass implementation
   (to raise :exc:`TypeError`).

   If *skipkeys* is false (the default), then it is a :exc:`TypeError` to
   attempt encoding of keys that are not str, int, long, float or None.  If
   *skipkeys* is true, such items are simply skipped.

   If *ensure_ascii* is true (the default), the output is guaranteed to be
   :class:`str` objects with all incoming unicode characters escaped.  If
   *ensure_ascii* is false, the output will be a unicode object.

   If *check_circular* is false (the default), then lists, dicts, and custom
   encoded objects will be checked for circular references during encoding to
   prevent an infinite recursion (which would cause an :exc:`OverflowError`).
   Otherwise, no such check takes place.

   If *allow_nan* is true (the default), then ``NaN``, ``Infinity``, and
   ``-Infinity`` will be encoded as such.  This behavior is not JSON
   specification compliant, but is consistent with most JavaScript based
   encoders and decoders.  Otherwise, it will be a :exc:`ValueError` to encode
   such floats.

   If *sort_keys* is true (not the default), then the output of dictionaries
   will be sorted by key; this is useful for regression tests to ensure that
   JSON serializations can be compared on a day-to-day basis.

   If *indent* is a string, then JSON array elements and object members
   will be pretty-printed with a newline followed by that string repeated
   for each level of nesting. ``None`` (the default) selects the most compact
   representation without any newlines. For backwards compatibility with
   versions of simplejson earlier than 2.1.0, an integer is also accepted
   and is converted to a string with that many spaces.

   .. versionchanged:: 2.1.0
      Changed *indent* from an integer number of spaces to a string.

   If specified, *separators* should be an ``(item_separator, key_separator)``
   tuple.  By default, ``(', ', ': ')`` are used.  To get the most compact JSON
   representation, you should specify ``(',', ':')`` to eliminate whitespace.

   If specified, *default* should be a function that gets called for objects
   that can't otherwise be serialized.  It should return a JSON encodable
   version of the object or raise a :exc:`TypeError`.

   If *encoding* is not ``None``, then all input strings will be transformed
   into unicode using that encoding prior to JSON-encoding.  The default is
   ``'utf-8'``.


   .. method:: default(o)

      Implement this method in a subclass such that it returns a serializable
      object for *o*, or calls the base implementation (to raise a
      :exc:`TypeError`).

      For example, to support arbitrary iterators, you could implement default
      like this::

         def default(self, o):
            try:
                iterable = iter(o)
            except TypeError:
                pass
            else:
                return list(iterable)
            return JSONEncoder.default(self, o)


   .. method:: encode(o)

      Return a JSON string representation of a Python data structure, *o*.  For
      example::

        >>> import simplejson as json
        >>> json.JSONEncoder().encode({"foo": ["bar", "baz"]})
        '{"foo": ["bar", "baz"]}'


   .. method:: iterencode(o)

      Encode the given object, *o*, and yield each string representation as
      available.  For example::

            for chunk in JSONEncoder().iterencode(bigobject):
                mysocket.write(chunk)

      Note that :meth:`encode` has much better performance than
      :meth:`iterencode`.

.. class:: JSONEncoderForHTML([skipkeys[, ensure_ascii[, check_circular[, allow_nan[, sort_keys[, indent[, separators[, encoding[, default]]]]]]]]])

   Subclass of :class:`JSONEncoder` that escapes &, <, and > for embedding in HTML.

   .. versionchanged:: 2.1.0
      New in 2.1.0
