use crate::instance::Instance;
use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use libloading::Library;
use std::error::Error;
use std::fmt;
use std::io;
use std::mem;
use std::os::raw::c_char;
use std::os::raw::c_void;
use std::ptr;
use std::sync::Arc;

#[cfg(windows)]
const LIB_PATH: &str = "vulkan-1.dll";

#[cfg(all(
    unix,
    not(any(target_os = "macos", target_os = "ios", target_os = "android"))
))]
const LIB_PATH: &str = "libvulkan.so.1";

#[cfg(target_os = "android")]
const LIB_PATH: &str = "libvulkan.so";

#[cfg(any(target_os = "macos", target_os = "ios"))]
const LIB_PATH: &str = "libvulkan.dylib";

/// Function loader
pub type Entry = EntryCustom<Arc<Library>>;

/// Function loader
#[derive(Clone)]
pub struct EntryCustom<L> {
    static_fn: vk::StaticFn,
    entry_fn_1_0: vk::EntryFnV1_0,
    entry_fn_1_1: vk::EntryFnV1_1,
    entry_fn_1_2: vk::EntryFnV1_2,
    lib: L,
}

#[derive(Debug)]
pub enum LoadingError {
    LibraryLoadError(io::Error),
}

impl fmt::Display for LoadingError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LoadingError::LibraryLoadError(e) => write!(f, "{}", e),
        }
    }
}

impl Error for LoadingError {}

#[derive(Clone, Debug)]
pub enum InstanceError {
    LoadError(Vec<&'static str>),
    VkError(vk::Result),
}

impl fmt::Display for InstanceError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            InstanceError::LoadError(e) => write!(f, "{}", e.join("; ")),
            InstanceError::VkError(e) => write!(f, "{}", e),
        }
    }
}

impl Error for InstanceError {}

#[allow(non_camel_case_types)]
pub trait EntryV1_0 {
    type Instance;
    fn fp_v1_0(&self) -> &vk::EntryFnV1_0;
    fn static_fn(&self) -> &vk::StaticFn;
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateInstance.html>"]
    ///
    /// # Safety
    /// In order for the created `Instance` to be valid for the duration of its
    /// usage, the `Entry` this was called on must be dropped later than the
    /// resulting `Instance`.
    unsafe fn create_instance(
        &self,
        create_info: &vk::InstanceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Self::Instance, InstanceError>;

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceLayerProperties.html>"]
    fn enumerate_instance_layer_properties(&self) -> VkResult<Vec<vk::LayerProperties>> {
        unsafe {
            let mut num = 0;
            self.fp_v1_0()
                .enumerate_instance_layer_properties(&mut num, ptr::null_mut());

            let mut v = Vec::with_capacity(num as usize);
            let err_code = self
                .fp_v1_0()
                .enumerate_instance_layer_properties(&mut num, v.as_mut_ptr());
            v.set_len(num as usize);
            match err_code {
                vk::Result::SUCCESS => Ok(v),
                _ => Err(err_code),
            }
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceExtensionProperties.html>"]
    fn enumerate_instance_extension_properties(&self) -> VkResult<Vec<vk::ExtensionProperties>> {
        unsafe {
            let mut num = 0;
            self.fp_v1_0().enumerate_instance_extension_properties(
                ptr::null(),
                &mut num,
                ptr::null_mut(),
            );
            let mut data = Vec::with_capacity(num as usize);
            let err_code = self.fp_v1_0().enumerate_instance_extension_properties(
                ptr::null(),
                &mut num,
                data.as_mut_ptr(),
            );
            data.set_len(num as usize);
            match err_code {
                vk::Result::SUCCESS => Ok(data),
                _ => Err(err_code),
            }
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetInstanceProcAddr.html>"]
    unsafe fn get_instance_proc_addr(
        &self,
        instance: vk::Instance,
        p_name: *const c_char,
    ) -> vk::PFN_vkVoidFunction {
        self.static_fn().get_instance_proc_addr(instance, p_name)
    }
}

impl<L> EntryV1_0 for EntryCustom<L> {
    type Instance = Instance;
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateInstance.html>"]
    ///
    /// # Safety
    /// In order for the created `Instance` to be valid for the duration of its
    /// usage, the `Entry` this was called on must be dropped later than the
    /// resulting `Instance`.
    unsafe fn create_instance(
        &self,
        create_info: &vk::InstanceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Self::Instance, InstanceError> {
        let mut instance: vk::Instance = mem::zeroed();
        let err_code = self.fp_v1_0().create_instance(
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut instance,
        );
        if err_code != vk::Result::SUCCESS {
            return Err(InstanceError::VkError(err_code));
        }
        Ok(Instance::load(&self.static_fn, instance))
    }
    fn fp_v1_0(&self) -> &vk::EntryFnV1_0 {
        &self.entry_fn_1_0
    }
    fn static_fn(&self) -> &vk::StaticFn {
        &self.static_fn
    }
}

#[allow(non_camel_case_types)]
pub trait EntryV1_1: EntryV1_0 {
    fn fp_v1_1(&self) -> &vk::EntryFnV1_1;

    #[deprecated = "This function is unavailable and therefore panics on Vulkan 1.0, please use `try_enumerate_instance_version` instead"]
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceVersion.html>"]
    fn enumerate_instance_version(&self) -> VkResult<u32> {
        unsafe {
            let mut api_version = 0;
            let err_code = self.fp_v1_1().enumerate_instance_version(&mut api_version);
            match err_code {
                vk::Result::SUCCESS => Ok(api_version),
                _ => Err(err_code),
            }
        }
    }
}

impl<L> EntryV1_1 for EntryCustom<L> {
    fn fp_v1_1(&self) -> &vk::EntryFnV1_1 {
        &self.entry_fn_1_1
    }
}

#[allow(non_camel_case_types)]
pub trait EntryV1_2: EntryV1_1 {
    fn fp_v1_2(&self) -> &vk::EntryFnV1_2;
}

impl<L> EntryV1_2 for EntryCustom<L> {
    fn fp_v1_2(&self) -> &vk::EntryFnV1_2 {
        &self.entry_fn_1_2
    }
}

impl EntryCustom<Arc<Library>> {
    /// ```rust,no_run
    /// use ash::{vk, Entry, version::EntryV1_0};
    /// # fn main() -> Result<(), Box<std::error::Error>> {
    /// let entry = Entry::new()?;
    /// let app_info = vk::ApplicationInfo {
    ///     api_version: vk::make_version(1, 0, 0),
    ///     ..Default::default()
    /// };
    /// let create_info = vk::InstanceCreateInfo {
    ///     p_application_info: &app_info,
    ///     ..Default::default()
    /// };
    /// let instance = unsafe { entry.create_instance(&create_info, None)? };
    /// # Ok(()) }
    /// ```
    pub fn new() -> Result<Entry, LoadingError> {
        Self::new_custom(
            || {
                Library::new(&LIB_PATH)
                    .map_err(LoadingError::LibraryLoadError)
                    .map(Arc::new)
            },
            |vk_lib, name| unsafe {
                vk_lib
                    .get(name.to_bytes_with_nul())
                    .map(|symbol| *symbol)
                    .unwrap_or(ptr::null_mut())
            },
        )
    }
}

impl<L> EntryCustom<L> {
    pub fn new_custom<Open, Load>(open: Open, mut load: Load) -> Result<Self, LoadingError>
    where
        Open: FnOnce() -> Result<L, LoadingError>,
        Load: FnMut(&mut L, &::std::ffi::CStr) -> *const c_void,
    {
        let mut lib = open()?;
        let static_fn = vk::StaticFn::load(|name| load(&mut lib, name));

        let entry_fn_1_0 = vk::EntryFnV1_0::load(|name| unsafe {
            mem::transmute(static_fn.get_instance_proc_addr(vk::Instance::null(), name.as_ptr()))
        });

        let entry_fn_1_1 = vk::EntryFnV1_1::load(|name| unsafe {
            mem::transmute(static_fn.get_instance_proc_addr(vk::Instance::null(), name.as_ptr()))
        });

        let entry_fn_1_2 = vk::EntryFnV1_2::load(|name| unsafe {
            mem::transmute(static_fn.get_instance_proc_addr(vk::Instance::null(), name.as_ptr()))
        });

        Ok(EntryCustom {
            static_fn,
            entry_fn_1_0,
            entry_fn_1_1,
            entry_fn_1_2,
            lib,
        })
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceVersion.html>"]
    /// ```rust,no_run
    /// # use ash::{Entry, vk};
    /// # fn main() -> Result<(), Box<std::error::Error>> {
    /// let entry = Entry::new()?;
    /// match entry.try_enumerate_instance_version()? {
    ///     // Vulkan 1.1+
    ///     Some(version) => {
    ///         let major = vk::version_major(version);
    ///         let minor = vk::version_minor(version);
    ///         let patch = vk::version_patch(version);
    ///     },
    ///     // Vulkan 1.0
    ///     None => {},
    /// }
    /// # Ok(()) }
    /// ```
    pub fn try_enumerate_instance_version(&self) -> VkResult<Option<u32>> {
        unsafe {
            let mut api_version = 0;
            let enumerate_instance_version: Option<vk::PFN_vkEnumerateInstanceVersion> = {
                let name = b"vkEnumerateInstanceVersion\0".as_ptr() as *const _;
                mem::transmute(
                    self.static_fn()
                        .get_instance_proc_addr(vk::Instance::null(), name),
                )
            };
            if let Some(enumerate_instance_version) = enumerate_instance_version {
                let err_code = (enumerate_instance_version)(&mut api_version);
                match err_code {
                    vk::Result::SUCCESS => Ok(Some(api_version)),
                    _ => Err(err_code),
                }
            } else {
                Ok(None)
            }
        }
    }
}
