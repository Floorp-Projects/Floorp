// Code to re-export the UniFFI scaffolding functions.
//
// Rust won't always re-export the functions from dependencies
// ([rust-lang#50007](https://github.com/rust-lang/rust/issues/50007))
//
// A workaround for this is to have the dependent crate reference a function from its dependency in
// an extern "C" function. This is clearly hacky and brittle, but at least we have some unittests
// that check if this works (fixtures/reexport-scaffolding-macro).
//
// The main way we use this macro is for that contain multiple UniFFI components (libxul,
// megazord).  The combined library has a cargo dependency for each component and calls
// uniffi_reexport_scaffolding!() for each one.

#[allow(missing_docs)]
#[doc(hidden)]
pub const fn uniffi_reexport_hack() {}

#[doc(hidden)]
#[macro_export]
macro_rules! uniffi_reexport_scaffolding {
    () => {
        #[doc(hidden)]
        #[no_mangle]
        pub extern "C" fn {{ ci.namespace() }}_uniffi_reexport_hack() {
            $crate::uniffi_reexport_hack()
        }
    };
}
