#[cfg(target_feature = "ssse3")]
pub mod teddy128;
#[cfg(not(target_feature = "ssse3"))]
#[path = "../simd_fallback/teddy128.rs"]
pub mod teddy128;
