pub use self::imp::*;

#[cfg(all(
    feature = "unstable",
    regex_runtime_teddy_ssse3,
    any(target_arch = "x86", target_arch = "x86_64"),
))]
mod imp;

#[cfg(not(all(
    feature = "unstable",
    regex_runtime_teddy_ssse3,
    any(target_arch = "x86", target_arch = "x86_64"),
)))]
#[path = "fallback.rs"]
mod imp;
