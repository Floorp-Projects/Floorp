# inflate
A [DEFLATE](http://www.gzip.org/zlib/rfc-deflate.html) decoder written in rust.

This library provides functionality to decompress data compressed with the DEFLATE algorithm,
both with and without a [zlib](https://tools.ietf.org/html/rfc1950) header/trailer.

# Examples
The easiest way to get `std::Vec<u8>` containing the decompressed bytes is to use either
`inflate::inflate_bytes` or `inflate::inflate_bytes_zlib` (depending on whether
the encoded data has zlib headers and trailers or not). The following example
decodes the DEFLATE encoded string "Hello, world" and prints it:

```rust
use inflate::inflate_bytes;
use std::str::from_utf8;

let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
let decoded = inflate_bytes(&encoded).unwrap();
println!("{}", from_utf8(&decoded).unwrap()); // prints "Hello, world"
```

If you need more flexibility, then the library also provides an implementation
of `std::io::Writer` in `inflate::writer`. Below is an example using an
`inflate::writer::InflateWriter` to decode the DEFLATE encoded string "Hello, world":

```rust
use inflate::InflateWriter;
use std::io::Write;
use std::str::from_utf8;

let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
let mut decoder = InflateWriter::new(Vec::new());
decoder.write(&encoded).unwrap();
let decoded = decoder.finish().unwrap();
println!("{}", from_utf8(&decoded).unwrap()); // prints "Hello, world"
```

Finally, if you need even more flexibility, or if you only want to depend on
`core`, you can use the `inflate::InflateStream` API. The below example
decodes an array of DEFLATE encoded bytes:

```rust
use inflate::InflateStream;

let data = [0x73, 0x49, 0x4d, 0xcb, 0x49, 0x2c, 0x49, 0x55, 0x00, 0x11, 0x00];
let mut inflater = InflateStream::new();
let mut out = Vec::<u8>::new();
let mut n = 0;
while n < data.len() {
    let res = inflater.update(&data[n..]);
    if let Ok((num_bytes_read, result)) = res {
        n += num_bytes_read;
        out.extend(result.iter().cloned());
    } else {
        res.unwrap();
    }
}
```
