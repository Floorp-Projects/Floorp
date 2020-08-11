[![Build Status][travis_badge]][travis]
[![Build Status][appveyor_badge]][appveyor]
[![Coverage Status][coveralls_badge]][coveralls]

## Common use-cases

This library provides the following common encodings:

- `HEXLOWER`: lowercase hexadecimal
- `HEXLOWER_PERMISSIVE`: lowercase hexadecimal with case-insensitive decoding
- `HEXUPPER`: uppercase hexadecimal
- `HEXUPPER_PERMISSIVE`: uppercase hexadecimal with case-insensitive decoding
- `BASE32`: RFC4648 base32
- `BASE32_NOPAD`: RFC4648 base32 without padding
- `BASE32_DNSSEC`: RFC5155 base32
- `BASE32_DNSCURVE`: DNSCurve base32
- `BASE32HEX`: RFC4648 base32hex
- `BASE32HEX_NOPAD`: RFC4648 base32hex without padding
- `BASE64`: RFC4648 base64
- `BASE64_NOPAD`: RFC4648 base64 without padding
- `BASE64_MIME`: RFC2045-like base64
- `BASE64URL`: RFC4648 base64url
- `BASE64URL_NOPAD`: RFC4648 base64url without padding

Typical usage looks like:

```rust
// allocating functions
BASE64.encode(&input_to_encode)
HEXLOWER.decode(&input_to_decode)
// in-place functions
BASE32.encode_mut(&input_to_encode, &mut encoded_output)
BASE64_URL.decode_mut(&input_to_decode, &mut decoded_output)
```

See the [documentation] or the [changelog] for more details.

## Custom use-cases

This library also provides the possibility to define custom little-endian ASCII
base-conversion encodings for bases of size 2, 4, 8, 16, 32, and 64 (for which
all above use-cases are particular instances). It supports:

- padded and unpadded encodings
- canonical encodings (e.g. trailing bits are checked)
- in-place encoding and decoding functions
- partial decoding functions (e.g. for error recovery)
- character translation (e.g. for case-insensitivity)
- most and least significant bit-order
- ignoring characters when decoding (e.g. for skipping newlines)
- wrapping the output when encoding

The typical definition of a custom encoding looks like:

```rust
lazy_static! {
    static ref HEX: Encoding = {
        let mut spec = Specification::new();
        spec.symbols.push_str("0123456789abcdef");
        spec.translate.from.push_str("ABCDEF");
        spec.translate.to.push_str("abcdef");
        spec.encoding().unwrap()
    };
    static ref BASE64: Encoding = {
        let mut spec = Specification::new();
        spec.symbols.push_str(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
        spec.padding = Some('=');
        spec.encoding().unwrap()
    };
}
```

You may also use the [macro] library to define a compile-time custom encoding:

```rust
const HEX: Encoding = new_encoding!{
    symbols: "0123456789abcdef",
    translate_from: "ABCDEF",
    translate_to: "abcdef",
};
const BASE64: Encoding = new_encoding!{
    symbols: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
    padding: '=',
};
```

See the [documentation] or the [changelog] for more details.

## Performance

The performance of the encoding and decoding functions (for both common and
custom encodings) are similar to existing implementations in C, Rust, and other
high-performance languages (see how to run the benchmarks on [github]).

## Swiss-knife binary

This crate is a library. If you are looking for the [binary] using this library,
see the installation instructions on [github].

[appveyor]: https://ci.appveyor.com/project/ia0/data-encoding
[appveyor_badge]:https://ci.appveyor.com/api/projects/status/wm4ga69xnlriukhl/branch/master?svg=true
[binary]: https://crates.io/crates/data-encoding-bin
[changelog]: https://github.com/ia0/data-encoding/blob/master/lib/CHANGELOG.md
[coveralls]: https://coveralls.io/github/ia0/data-encoding?branch=master
[coveralls_badge]: https://coveralls.io/repos/github/ia0/data-encoding/badge.svg?branch=master
[documentation]: https://docs.rs/data-encoding
[github]: https://github.com/ia0/data-encoding
[macro]: https://crates.io/crates/data-encoding-macro
[travis]: https://travis-ci.org/ia0/data-encoding
[travis_badge]: https://travis-ci.org/ia0/data-encoding.svg?branch=master
