1.1.0
=====
This release of `byteorder` features a number of fixes and improvements, mostly
as a result of the
[Litz Blitz evaluation](https://public.etherpad-mozilla.org/p/rust-crate-eval-byteorder).

Feature enhancements:

* [FEATURE #63](https://github.com/BurntSushi/byteorder/issues/63):
  Add methods for reading/writing slices of numbers for a specific
  endianness.
* [FEATURE #65](https://github.com/BurntSushi/byteorder/issues/65):
  Add support for `u128`/`i128` types. (Behind the nightly only `i128`
  feature.)
* [FEATURE #72](https://github.com/BurntSushi/byteorder/issues/72):
  Add "panics" and "errors" sections for each relevant public API item.
* [FEATURE #74](https://github.com/BurntSushi/byteorder/issues/74):
  Add CI badges to Cargo.toml.
* [FEATURE #75](https://github.com/BurntSushi/byteorder/issues/75):
  Add more examples to public API items.
* Add 24-bit read/write methods.
* Add `BE` and `LE` type aliases for `BigEndian` and `LittleEndian`,
  respectively.

Bug fixes:

* [BUG #68](https://github.com/BurntSushi/byteorder/issues/68):
  Panic in {BigEndian,LittleEndian}::default.
* [BUG #69](https://github.com/BurntSushi/byteorder/issues/69):
  Seal the `ByteOrder` trait to prevent out-of-crate implementations.
* [BUG #71](https://github.com/BurntSushi/byteorder/issues/71):
  Guarantee that the results of `read_f32`/`read_f64` are always defined.
* [BUG #73](https://github.com/BurntSushi/byteorder/issues/73):
  Add crates.io categories.
* [BUG #77](https://github.com/BurntSushi/byteorder/issues/77):
  Add `html_root` doc attribute.
