//! Features specific to x86 and x86-64 CPUs.

#[cfg(any(feature = "doc", target_feature = "sse2"))]
pub mod sse2;
#[cfg(any(feature = "doc", target_feature = "sse3"))]
pub mod sse3;
#[cfg(any(feature = "doc", target_feature = "ssse3"))]
pub mod ssse3;
#[cfg(any(feature = "doc", target_feature = "sse4.1"))]
pub mod sse4_1;
#[cfg(any(feature = "doc", target_feature = "sse4.2"))]
pub mod sse4_2;
#[cfg(any(feature = "doc", target_feature = "avx"))]
pub mod avx;
#[cfg(any(feature = "doc", target_feature = "avx2"))]
pub mod avx2;
