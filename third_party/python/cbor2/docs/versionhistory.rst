Version history
===============

This library adheres to `Semantic Versioning <http://semver.org/>`_.

**4.0.1.** (2017-08-21)

- Fixed silent truncation of decoded data if there are not enough bytes in the stream for an exact
  read (``CBORDecodeError`` is now raised instead)

**4.0.0** (2017-04-24)

- **BACKWARD INCOMPATIBLE** Value sharing has been disabled by default, for better compatibility
  with other implementations and better performance (since it is rarely needed)
- **BACKWARD INCOMPATIBLE** Replaced the ``semantic_decoders`` decoder option with the ``tag_hook``
  option
- **BACKWARD INCOMPATIBLE** Replaced the ``encoders`` encoder option with the ``default`` option
- **BACKWARD INCOMPATIBLE** Factored out the file object argument (``fp``) from all callbacks
- **BACKWARD INCOMPATIBLE** The encoder no longer supports every imaginable type implementing the
  ``Sequence`` or ``Map`` interface, as they turned out to be too broad
- Added the ``object_hook`` option for decoding dicts into complex objects
  (intended for situations where JSON compatibility is required and semantic tags cannot be used)
- Added encoding and decoding of simple values (``CBORSimpleValue``)
  (contributed by Jerry Lundström)
- Replaced the decoder for bignums with a simpler and faster version (contributed by orent)
- Made all relevant classes and functions available directly in the ``cbor2`` namespace
- Added proper documentation

**3.0.4** (2016-09-24)

- Fixed TypeError when trying to encode extension types (regression introduced in 3.0.3)

**3.0.3** (2016-09-23)

- No changes, just re-releasing due to git tagging screw-up

**3.0.2** (2016-09-23)

- Fixed decoding failure for datetimes with microseconds (tag 0)

**3.0.1** (2016-08-08)

- Fixed error in the cyclic structure detection code that could mistake one container for
  another, sometimes causing a bogus error about cyclic data structures where there was none

**3.0.0** (2016-07-03)

- **BACKWARD INCOMPATIBLE** Encoder callbacks now receive three arguments: the encoder instance,
  the value to encode and a file-like object. The callback must must now either write directly to
  the file-like object or call another encoder callback instead of returning an iterable.
- **BACKWARD INCOMPATIBLE** Semantic decoder callbacks now receive four arguments: the decoder
  instance, the primitive value, a file-like object and the shareable index for the decoded value.
  Decoders that support value sharing must now set the raw value at the given index in
  ``decoder.shareables``.
- **BACKWARD INCOMPATIBLE** Removed support for iterative encoding (``CBOREncoder.encode()`` is no
  longer a generator function and always returns ``None``)
- Significantly improved performance (encoder ~30 % faster, decoder ~60 % faster)
- Fixed serialization round-trip for ``undefined`` (simple type #23)
- Added proper support for value sharing in callbacks

**2.0.0** (2016-06-11)

- **BACKWARD INCOMPATIBLE** Deserialize unknown tags as ``CBORTag`` objects so as not to lose
  information
- Fixed error messages coming from nested structures

**1.1.0** (2016-06-10)

- Fixed deserialization of cyclic structures

**1.0.0** (2016-06-08)

- Initial release
