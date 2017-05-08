# Generating Bindings to C++

`bindgen` can handle a surprising number of C++ features, but not all of
them. When `bindgen` can't translate some C++ construct into Rust, it usually
comes down to one of two things:

1. Rust has no equivalent language feature
2. C++ is *hard!*

Notable C++ features that are unsupported or only partially supported, and for
which `bindgen` *should* generate opaque blobs whenever it finds an occurrence
of them in a type it is generating bindings for:

* Template specialization
* Partial template specialization
* Traits templates
* SFINAE

When passing in header files, the file will automatically be treated as C++ if
it ends in `.hpp`. If it doesn't, adding `-x=c++` clang args can be used to
force C++ mode. You probably also want to use `-std=c++14` or similar clang args
as well.

You pretty much **must** use [whitelisting](./whitelisting.html) when working
with C++ to avoid pulling in all of the `std::*` types, many of which `bindgen`
cannot handle. Additionally, you may want to mark other types
as [opaque](./opaque.html) that `bindgen` stumbles on.
