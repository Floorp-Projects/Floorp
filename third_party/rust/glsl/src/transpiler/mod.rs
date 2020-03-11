//! GLSL transpilers – i.e. going from GLSL to anything else.
//!
//! There’s no public interface / trait to define what a transpiler is. It depends on the target
//! representation you aim.

pub mod glsl;
#[cfg(feature = "spirv")]
pub mod spirv;
