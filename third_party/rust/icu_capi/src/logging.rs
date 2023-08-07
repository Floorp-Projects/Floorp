// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    #[diplomat::opaque]
    /// An object allowing control over the logging used
    pub struct ICU4XLogger;

    impl ICU4XLogger {
        /// Initialize the logger from the `simple_logger` crate, which simply logs to
        /// stdout. Returns `false` if there was already a logger set, or if logging has not been
        /// compiled into the platform
        pub fn init_simple_logger() -> bool {
            #[cfg(not(all(not(target_arch = "wasm32"), feature = "simple_logger")))]
            unimplemented!("ICU4X binary not compiled with simple_logger support");
            #[cfg(all(not(target_arch = "wasm32"), feature = "simple_logger"))]
            simple_logger::init().is_ok()
        }
    }
}
