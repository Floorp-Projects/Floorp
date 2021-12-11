//! Provides a debug/trace output with json format.
//! This is disabled by default, and can be enabled by "logging" feature.

#[cfg(feature = "logging")]
pub use log::{debug, trace};
#[cfg(feature = "logging")]
pub use serde_json::json;

#[cfg(not(feature = "logging"))]
#[macro_export]
macro_rules! json_debug {
    ($($t:tt)*) => {};
}

#[cfg(feature = "logging")]
#[macro_export(local_inner_macros)]
macro_rules! json_debug {
    ($($t:tt)*) => {
        debug!(
            "{}",
            json!($($t)*)
        );
    };
}

#[cfg(not(feature = "logging"))]
#[macro_export]
macro_rules! json_trace {
    ($($t:tt)*) => {};
}

#[cfg(feature = "logging")]
#[macro_export(local_inner_macros)]
macro_rules! json_trace {
    ($($t:tt)*) => {
        trace!(
            "{}",
            json!($($t)*)
        );
    };
}
