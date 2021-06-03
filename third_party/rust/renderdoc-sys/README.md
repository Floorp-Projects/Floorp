# renderdoc-sys

[![Build Status][build-badge]][build-url]
[![Crates.io][crate-badge]][crate-url]
[![Documentation][docs-badge]][docs-url]

[build-badge]: https://circleci.com/gh/ebkalderon/renderdoc-rs.svg?style=shield
[build-url]: https://circleci.com/gh/ebkalderon/renderdoc-rs/tree/master/renderdoc-sys
[crate-badge]: https://img.shields.io/crates/v/renderdoc-sys.svg
[crate-url]: https://crates.io/crates/renderdoc-sys
[docs-badge]: https://docs.rs/renderdoc-sys/badge.svg
[docs-url]: https://docs.rs/renderdoc-sys

Low-level Rust FFI bindings to [RenderDoc], a popular graphics debugger.

[RenderDoc]: https://renderdoc.org/

RenderDoc is a free and open source graphics debugging tool. RenderDoc allows
game developers to take frame captures of their applications, replay them,
examine the graphics pipeline state, and potentially identify nasty graphics
bugs.

These raw bindings are generated directly from the [renderdoc_app.h][header]
file provided upstream. This crate does not provide nor dynamically link to the
required `renderdoc.dll` or `librenderdoc.so` library itself; it only provides
the FFI objects for the [in-application RenderDoc API][api].

[header]: https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/api/app/renderdoc_app.h
[api]: https://renderdoc.org/docs/in_application_api.html
