spirv-headers of the rspirv project
===================================

[![Crate][img-crate-headers]][crate-headers]
[![Documentation][img-doc-headers]][doc-headers]

The headers crate for the rspirv project which provides Rust definitions of
SPIR-V structs, enums, and constants.

Usage
-----

This project uses associated constants, which became available in the stable channel
since [1.20][rust-1.20]. So to compile with a compiler from the stable channel,
please make sure that the version is >= 1.20.

First add to your `Cargo.toml`:

```toml
[dependencies]
spirv = "0.3.0"
```

Version
-------

Note that the major and minor version of this create is tracking the SPIR-V spec,
while the patch number is used for bugfixes for the crate itself. So version
`1.4.2` is tracking SPIR-V 1.4 but not necessarily revision 2. Major client APIs
like Vulkan/OpenCL pin to a specific major and minor version, regardless of the
revision.

Examples
--------

Please see the [documentation][doc-headers] and project's
[README][project-readme] for examples.

[img-crate-headers]: https://img.shields.io/crates/v/spirv.svg
[img-doc-headers]: https://docs.rs/spirv/badge.svg
[crate-headers]: https://crates.io/crates/spirv
[doc-headers]: https://docs.rs/spirv
[project-readme]: https://github.com/gfx-rs/rspirv/blob/master/README.md
[rust-1.20]: https://blog.rust-lang.org/2017/08/31/Rust-1.20.html
