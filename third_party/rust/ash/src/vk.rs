#![allow(
    clippy::too_many_arguments,
    clippy::cognitive_complexity,
    clippy::wrong_self_convention
)]
#[macro_use]
mod macros;
pub use macros::*;
mod aliases;
pub use aliases::*;
mod bitflags;
pub use bitflags::*;
mod const_debugs;
pub(crate) use const_debugs::*;
mod constants;
pub use constants::*;
mod definitions;
pub use definitions::*;
mod enums;
pub use enums::*;
mod extensions;
pub use extensions::*;
mod feature_extensions;
pub use feature_extensions::*;
mod features;
pub use features::*;
mod platform_types;
pub use platform_types::*;
#[doc = r" Iterates through the pointer chain. Includes the item that is passed into the function."]
#[doc = r" Stops at the last `BaseOutStructure` that has a null `p_next` field."]
pub(crate) unsafe fn ptr_chain_iter<T>(ptr: &mut T) -> impl Iterator<Item = *mut BaseOutStructure> {
    let ptr: *mut BaseOutStructure = ptr as *mut T as _;
    (0..).scan(ptr, |p_ptr, _| {
        if p_ptr.is_null() {
            return None;
        }
        let n_ptr = (**p_ptr).p_next as *mut BaseOutStructure;
        let old = *p_ptr;
        *p_ptr = n_ptr;
        Some(old)
    })
}
pub trait Handle {
    const TYPE: ObjectType;
    fn as_raw(self) -> u64;
    fn from_raw(_: u64) -> Self;
}
