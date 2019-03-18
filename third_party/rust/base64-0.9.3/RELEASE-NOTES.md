# 0.9.3

- Update safemem

# 0.9.2

- Derive `Clone` for `DecodeError`.

# 0.9.1

- Add support for `crypt(3)`'s base64 variant.

# 0.9.0

- `decode_config_slice` function for no-allocation decoding, analogous to `encode_config_slice`
- Decode performance optimization

# 0.8.0

- `encode_config_slice` function for no-allocation encoding

# 0.7.0

- `STANDARD_NO_PAD` config
- `Base64Display` heap-free wrapper for use in format strings, etc

# 0.6.0

- Decode performance improvements
- Use `unsafe` in fewer places
- Added fuzzers

# 0.5.2

- Avoid usize overflow when calculating length
- Better line wrapping performance

# 0.5.1

- Temporarily disable line wrapping
- Add Apache 2.0 license

# 0.5.0

- MIME support, including configurable line endings and line wrapping
- Removed `decode_ws`
- Renamed `Base64Error` to `DecodeError`

# 0.4.1

- Allow decoding a `AsRef<[u8]>` instead of just a `&str`

# 0.4.0

- Configurable padding
- Encode performance improvements

# 0.3.0

- Added encode/decode functions that do not allocate their own storage
- Decode performance improvements
- Extraneous padding bytes are no longer ignored. Now, an error will be returned.
