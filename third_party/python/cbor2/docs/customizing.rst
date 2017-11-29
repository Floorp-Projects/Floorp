Customizing encoding and decoding
=================================

Both the encoder and decoder can be customized to support a wider range of types.

On the encoder side, this is accomplished by passing a callback as the ``default`` constructor
argument. This callback will receive an object that the encoder could not serialize on its own.
The callback should then return a value that the encoder can serialize on its own, although the
return value is allowed to contain objects that also require the encoder to use the callback, as
long as it won't result in an infinite loop.

On the decoder side, you have two options: ``tag_hook`` and ``object_hook``. The former is called
by the decoder to process any semantic tags that have no predefined decoders. The latter is called
for any newly decoded ``dict`` objects, and is mostly useful for implementing a JSON compatible
custom type serialization scheme. Unless your requirements restrict you to JSON compatible types
only, it is recommended to use ``tag_hook`` for this purpose.

JSON compatibility
------------------

In certain applications, it may be desirable to limit the supported types to the same ones
serializable as JSON: (unicode) string, integer, float, boolean, null, array and object (dict).
This can be done by passing the ``json_compatible`` option to the encoder. When incompatible types
are encountered, a :class:`~cbor2.encoder.CBOREncodeError` is then raised.

For the decoder, there is no support for detecting incoming incompatible types yet.

Using the CBOR tags for custom types
------------------------------------

The most common way to use ``default`` is to call :meth:`~cbor2.encoder.CBOREncoder.encode`
to add a custom tag in the data stream, with the payload as the value::

    class Point(object):
        def __init__(self, x, y):
            self.x = x
            self.y = y

    def default_encoder(encoder, value):
        # Tag number 4000 was chosen arbitrarily
        encoder.encode(CBORTag(4000, [value.x, value.y]))

The corresponding ``tag_hook`` would be::

    def tag_hook(decoder, tag, shareable_index=None):
        if tag.tag != 4000:
            return tag

        # tag.value is now the [x, y] list we serialized before
        return Point(*tag.value)

Using dicts to carry custom types
---------------------------------

The same could be done with ``object_hook``, except less efficiently::

    def default_encoder(encoder, value):
        encoder.encode(dict(typename='Point', x=value.x, y=value.y))

    def object_hook(decoder, value):
        if value.get('typename') != 'Point':
            return value

        return Point(value['x'], value['y'])

You should make sure that whatever way you decide to use for telling apart your "specially marked"
dicts from arbitrary data dicts won't mistake on for the other.

Value sharing with custom types
-------------------------------

In order to properly encode and decode cyclic references with custom types, some special care has
to be taken. Suppose you have a custom type as below, where every child object contains a reference
to its parent and the parent contains a list of children::

    from cbor2 import dumps, loads, shareable_encoder, CBORTag


    class MyType(object):
        def __init__(self, parent=None):
            self.parent = parent
            self.children = []
            if parent:
                self.parent.children.append(self)

This would not normally be serializable, as it would lead to an endless loop (in the worst case)
and raise some exception (in the best case). Now, enter CBOR's extension tags 28 and 29. These tags
make it possible to add special markers into the data stream which can be later referenced and
substituted with the object marked earlier.

To do this, in ``default`` hooks used with the encoder you will need to use the
:meth:`~cbor2.encoder.shareable_encoder` decorator on your ``default`` hook function. It will
automatically automatically add the object to the shared values registry on the encoder and prevent
it from being serialized twice (instead writing a reference to the data stream)::

    @shareable_encoder
    def default_encoder(encoder, value):
        # The state has to be serialized separately so that the decoder would have a chance to
        # create an empty instance before the shared value references are decoded
        serialized_state = encoder.encode_to_bytes(value.__dict__)
        encoder.encode(CBORTag(3000, serialized_state))

On the decoder side, you will need to initialize an empty instance for shared value lookup before
the object's state (which may contain references to it) is decoded.
This is done with the :meth:`~cbor2.encoder.CBORDecoder.set_shareable` method::

    def tag_hook(decoder, tag, shareable_index=None):
        # Return all other tags as-is
        if tag.tag != 3000:
            return tag

        # Create a raw instance before initializing its state to make it possible for cyclic
        # references to work
        instance = MyType.__new__(MyType)
        decoder.set_shareable(shareable_index, instance)

        # Separately decode the state of the new object and then apply it
        state = decoder.decode_from_bytes(tag.value)
        instance.__dict__.update(state)
        return instance

You could then verify that the cyclic references have been restored after deserialization::

    parent = MyType()
    child1 = MyType(parent)
    child2 = MyType(parent)
    serialized = dumps(parent, default=default_encoder, value_sharing=True)

    new_parent = loads(serialized, tag_hook=tag_hook)
    assert new_parent.children[0].parent is new_parent
    assert new_parent.children[1].parent is new_parent

