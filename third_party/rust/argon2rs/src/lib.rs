#![cfg_attr(feature = "simd", feature(repr_simd, platform_intrinsics))]

mod octword;
#[macro_use]
mod block;
mod argon2;
pub mod verifier;

pub use argon2::{Argon2, ParamErr, Variant, argon2d_simple, argon2i_simple,
                 defaults};
