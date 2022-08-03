This directory contains generated code for the UniFFI examples/fixtures and JS
unit tests for it.

This is only built if the `--enable-uniffi-fixtures` flag is present in
`mozconfig`.  There's no benefit to including this in a release build.

To add additional examples/fixtures:
  - For most of these steps, find the code for existing fixtures and use it as a template for the new code.
  - Edit `toolkit/components/uniffi-bindgen-gecko-js/mach_commands.py`
    - Add an entry to `FIXTURE_UDL_FILES`
  - Edit `toolkit/library/rust/shared/Cargo.toml`
    - Add an optional dependency for the fixture.
    - Add the feature to the list of features enabled by `uniffi_fixtures`.
  - Edit `toolkit/library/rust/shared/lib.rs`:
     - Add an `extern crate [name]` to the `uniffi_fixtures` mod
       - Note: [name] is the name from the `[lib]` section in the Cargo.toml
         for the example/fixture crate.  This does not always match the package
         name for the crate.
     - Add `[name]::reexport_uniffi_scaffolding` to the `uniffi_fixtures` mod
  - Edit `toolkit/components/uniffi-bindgen-gecko-js/fixtures/moz.build` and add the fixture name to the `components`
    list.
  - Add a test module to the `toolkit/components/uniffi-bindgen-gecko-js/fixtures/tests/` directory and an entry for it
    in `toolkit/components/uniffi-bindgen-gecko-js/fixtures/tests/xpcshell.ini`
  - Run `mach vendor rust` to vendor in the Rust code.
  - Run `mach uniffi generate` to generate the scaffolding code.
  - Check in any new files

To run the tests:
  - Make sure you have a `mozconfig` file containing the line `ac_add_options --enable-uniffi-fixtures`
  - Run `mach uniffi generate` if:
    - You've added or updated a fixture
    - You've made changes to `uniffi-bindgen-gecko-js`
  - Run `mach build`
  - Run `mach xpcshell-test toolkit/components/uniffi-bindgen-gecko-js/fixtures/tests/`
    - You can also use a path to specific test file
    - For subsequent runs, if you only modify the test files, then you can re-run this step directly
