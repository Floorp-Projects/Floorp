//! Features specific to ARM CPUs.

#[cfg(any(feature = "doc", target_feature = "neon"))]
pub mod neon;
