Basic usage
===========

Serializing and deserializing with cbor2 is pretty straightforward::

    from cbor2 import dumps, loads

    # Serialize an object as a bytestring
    data = dumps(['hello', 'world'])

    # Deserialize a bytestring
    obj = loads(data)

    # Efficiently deserialize from a file
    with open('input.cbor', 'rb') as fp:
        obj = load(fp)

    # Efficiently serialize an object to a file
    with open('output.cbor', 'wb') as fp:
        dump(obj, fp)

Some data types, however, require extra considerations, as detailed below.

String/bytes handling on Python 2
---------------------------------

The ``str`` type is encoded as binary on Python 2. If you want to encode strings as text on
Python 2, use unicode strings instead.

Date/time handling
------------------

The CBOR specification does not support naïve datetimes (that is, datetimes where ``tzinfo`` is
missing). When the encoder encounters such a datetime, it needs to know which timezone it belongs
to. To this end, you can specify a default timezone by passing a :class:`~datetime.tzinfo` instance
to :func:`~cbor2.encoder.dump`/:func:`~cbor2.encoder.dumps` call as the ``timezone`` argument.
Decoded datetimes are always timezone aware.

By default, datetimes are serialized in a manner that retains their timezone offsets. You can
optimize the data stream size by passing ``datetime_as_timestamp=False`` to
:func:`~cbor2.encoder.dump`/:func:`~cbor2.encoder.dumps`, but this causes the timezone offset
information to be lost.

Cyclic (recursive) data structures
----------------------------------

If the encoder encounters a shareable object (ie. list or dict) that it has been before, it will
by default raise :exc:`~cbor2.encoder.CBOREncodeError` indicating that a cyclic reference has been
detected and value sharing was not enabled. CBOR has, however, an extension specification that
allows the encoder to reference a previously encoded value without processing it again. This makes
it possible to serialize such cyclic references, but value sharing has to be enabled by passing
``value_sharing=True`` to :func:`~cbor2.encoder.dump`/:func:`~cbor2.encoder.dumps`.

.. warning:: Support for value sharing is rare in other CBOR implementations, so think carefully
    whether you want to enable it. It also causes some line overhead, as all potentially shareable
    values must be tagged as such.

Tag support
-----------

In addition to all standard CBOR tags, this library supports many extended tags:

=== ======================================== ====================================================
Tag Semantics                                Python type(s)
=== ======================================== ====================================================
0   Standard date/time string                datetime.date / datetime.datetime
1   Epoch-based date/time                    datetime.date / datetime.datetime
2   Positive bignum                          int / long
3   Negative bignum                          int / long
4   Decimal fraction                         decimal.Decimal
5   Bigfloat                                 decimal.Decimal
28  Mark shared value                        N/A
29  Reference shared value                   N/A
30  Rational number                          fractions.Fraction
35  Regular expression                       ``_sre.SRE_Pattern`` (result of ``re.compile(...)``)
36  MIME message                             email.message.Message
37  Binary UUID                              uuid.UUID
=== ======================================== ====================================================

Arbitary tags can be represented with the :class:`~cbor2.types.CBORTag` class.
