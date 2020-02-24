# `bytemuck` changelog

## 1.2.0

* [thomcc](https://github.com/thomcc) added many things:
  * A fully sound `offset_of!` macro [#10](https://github.com/Lokathor/bytemuck/pull/10)
  * A `Contiguous` trait for when you've got enums with declared values
    all in a row [#12](https://github.com/Lokathor/bytemuck/pull/12)
  * A `TransparentWrapper` marker trait for when you want to more clearly
    enable adding and removing a wrapper struct to its inner value
    [#15](https://github.com/Lokathor/bytemuck/pull/15)
  * Now MIRI is run on CI in every sigle push!
    [#16](https://github.com/Lokathor/bytemuck/pull/16)

## 1.1.0

* [SimonSapin](https://github.com/SimonSapin) added `from_bytes`,
  `from_bytes_mut`, `try_from_bytes`, and `try_from_bytes_mut` ([PR
  Link](https://github.com/Lokathor/bytemuck/pull/8))

## 1.0.1

* Changed to the [zlib](https://opensource.org/licenses/Zlib) license.
* Added much more proper documentation.
* Reduced the minimum Rust version to 1.34
