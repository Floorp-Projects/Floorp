#![deny(clippy::use_self)]
#![warn(trivial_casts, trivial_numeric_casts)]
#![allow(
    clippy::too_many_arguments,
    clippy::missing_safety_doc,
    clippy::upper_case_acronyms
)]
#![cfg_attr(docsrs, feature(doc_cfg))]
//! # Vulkan API
//!
//! <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/index.html>
//!
//! ## Examples
//!
//! ```no_run
//! use ash::{vk, Entry};
//! # fn main() -> Result<(), Box<dyn std::error::Error>> {
//! let entry = Entry::linked();
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
//!
//! Load the Vulkan library linked at compile time using [`Entry::linked()`], or load it at runtime
//! using [`Entry::load()`], which uses `libloading`. If you want to perform entry point loading
//! yourself, call [`Entry::from_static_fn()`].

pub use crate::device::Device;
pub use crate::entry::Entry;
#[cfg(feature = "loaded")]
pub use crate::entry::LoadingError;
pub use crate::instance::Instance;

mod device;
mod entry;
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
            Some(inner) => inner,
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
            <*mut _>::cast(&mut variable_pointers),
            <*mut _>::cast(&mut corner),
        ];
        let mut device_create_info = vk::DeviceCreateInfo::builder()
            .push_next(&mut corner)
            .push_next(&mut variable_pointers);
        let chain2: Vec<*mut vk::BaseOutStructure> = unsafe {
            vk::ptr_chain_iter(&mut device_create_info)
                .skip(1)
                .collect()
        };
        assert_eq!(chain, chain2);
    }
}
