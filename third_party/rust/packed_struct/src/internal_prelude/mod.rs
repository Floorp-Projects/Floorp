#[cfg(not(feature="std"))]
#[path = "no_std.rs"]
pub mod v1;

#[cfg(feature="std")]
#[path = "std.rs"]
pub mod v1;