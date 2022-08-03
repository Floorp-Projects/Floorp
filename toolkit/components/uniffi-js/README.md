# uniffi-js

This directory contains C++ helper code for the UniFFI Rust library
(https://github.com/mozilla/uniffi-rs/).

 - `UniFFIPointer.*` and `UniFFIPointerType.*` implement the `UniFFIPointer` WebIDL class

 - `UniFFI*Scaffolding.cpp` implements the `UniFFIScaffolding` WebIDL class.
   - UniFFIGeneratedScaffolding.cpp contains the generated code for all
     non-testing UDL files.
   - UniFFIFixtureScaffolding.cpp contains generated code for test fixture UDL
     files. It's only compiled if `--enable-uniffi-fixtures` is set.
   - UniFFIScaffolding.cpp is a facade that wraps UniFFIFixtureScaffolding, and
     UniFFIGeneratedScaffolding if enabled, to implement the interface.

- `ScaffoldingConverter.h`, `ScaffoldingCall.h` contain generic code to make
  the scaffolding calls.  In general, we try to keep the logic of the calls in
  these files and limit the generated code to routing call IDs to template
  classes defined here.

- `OwnedRustBuffer.*` implements a C++ class to help manager ownership of a RustBuffer.

- `UniFFIRust.h` contains definitions for the C functions that UniFFI exports.
