#![allow(
    clippy::too_many_arguments,
    clippy::missing_safety_doc,
    clippy::upper_case_acronyms
)]
//! # Vulkan API
//!
//! <https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/index.html>
//!
//! ## Examples
//!
//! ```no_run
//! use ash::{vk, Entry};
//! # fn main() -> Result<(), Box<dyn std::error::Error>> {
//! let entry = unsafe { Entry::new() }?;
//! let app_info = vk::ApplicationInfo {
//!     api_version: vk::make_api_version(0, 1, 0, 0),
//!     ..Default::default()
//! };
//! let create_info = vk::InstanceCreateInfo {
//!     p_application_info: &app_info,
//!     ..Default::default()
//! };
//! let instance = unsafe { entry.create_instance(&create_info, None)? };
//! # Ok(()) }
//! ```
//!
//! ## Getting started
//! Load the Vulkan library at the default location using [`Entry::new()`][EntryCustom<_>::new()],
//! or at a custom location using [`Entry::with_library("path/to/vulkan")`][EntryCustom<_>::with_library()].
//! These loaders use [`libloading`]. If you wish to perform function loading yourself
//! call [`EntryCustom::new_custom()`] with a closure turning function names
//! into function pointers.

pub use crate::device::Device;
pub use crate::entry::{EntryCustom, InstanceError};
#[cfg(feature = "libloading")]
pub use crate::entry_libloading::{Entry, LoadingError};
pub use crate::instance::Instance;

mod device;
mod entry;
#[cfg(feature = "libloading")]
mod entry_libloading;
mod instance;
pub mod prelude;
pub mod util;
/// Raw Vulkan bindings and types, generated from `vk.xml`
#[macro_use]
pub mod vk;

// macros of vk need to be defined beforehand
/// Wrappers for Vulkan extensions
pub mod extensions;

pub trait RawPtr<T> {
    fn as_raw_ptr(&self) -> *const T;
}

impl<'r, T> RawPtr<T> for Option<&'r T> {
    fn as_raw_ptr(&self) -> *const T {
        match *self {
            Some(inner) => inner as *const T,

            _ => ::std::ptr::null(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::vk;
    #[test]
    fn test_ptr_chains() {
        let mut variable_pointers = vk::PhysicalDeviceVariablePointerFeatures::builder();
        let mut corner = vk::PhysicalDeviceCornerSampledImageFeaturesNV::builder();
        let chain = vec![
            &variable_pointers as *const _ as usize,
            &corner as *const _ as usize,
        ];
        let mut device_create_info = vk::DeviceCreateInfo::builder()
            .push_next(&mut corner)
            .push_next(&mut variable_pointers);
        let chain2: Vec<usize> = unsafe {
            vk::ptr_chain_iter(&mut device_create_info)
                .skip(1)
                .map(|ptr| ptr as usize)
                .collect()
        };
        assert_eq!(chain, chain2);
    }
}
