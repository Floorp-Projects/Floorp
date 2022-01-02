#![warn(missing_docs)]

//! Helper extensions of standard containers that allow memcopy-less operation.

pub use self::{
    boxed::{BoxAllocation, BoxHelper},
    vec::{VecAllocation, VecEntry, VecHelper},
};

mod boxed;
mod vec;
