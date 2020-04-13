#![allow(clippy::too_many_arguments, clippy::missing_safety_doc)]
//! # Vulkan API
//!
//! <https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/index.html>
//!
//! ## Examples
//!
//! ```rust,no_run
//! use ash::{vk, Entry, version::EntryV1_0};
//! # fn main() -> Result<(), Box<dyn std::error::Error>> {
//! let entry = Entry::new()?;
//! let app_info = vk::ApplicationInfo {
//!     api_version: vk::make_version(1, 0, 0),
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

pub use crate::device::Device;
pub use crate::entry::{Entry, EntryCustom, InstanceError, LoadingError};
pub use crate::instance::Instance;

mod device;
mod entry;
mod instance;
pub mod prelude;
pub mod util;
pub mod version;
#[macro_use]
pub mod vk;

// macros of vk need to be defined beforehand
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
