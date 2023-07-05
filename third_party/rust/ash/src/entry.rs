use crate::instance::Instance;
use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
#[cfg(feature = "loaded")]
use std::ffi::OsStr;
use std::mem;
use std::os::raw::c_char;
use std::os::raw::c_void;
use std::ptr;
#[cfg(feature = "loaded")]
use std::sync::Arc;

#[cfg(feature = "loaded")]
use libloading::Library;

/// Holds the Vulkan functions independent of a particular instance
#[derive(Clone)]
pub struct Entry {
    static_fn: vk::StaticFn,
    entry_fn_1_0: vk::EntryFnV1_0,
    entry_fn_1_1: vk::EntryFnV1_1,
    entry_fn_1_2: vk::EntryFnV1_2,
    entry_fn_1_3: vk::EntryFnV1_3,
    #[cfg(feature = "loaded")]
    _lib_guard: Option<Arc<Library>>,
}

/// Vulkan core 1.0
#[allow(non_camel_case_types)]
impl Entry {
    /// Load default Vulkan library for the current platform
    ///
    /// Prefer this over [`linked`](Self::linked) when your application can gracefully handle
    /// environments that lack Vulkan support, and when the build environment might not have Vulkan
    /// development packages installed (e.g. the Vulkan SDK, or Ubuntu's `libvulkan-dev`).
    ///
    /// # Safety
    ///
    /// `dlopen`ing native libraries is inherently unsafe. The safety guidelines
    /// for [`Library::new()`] and [`Library::get()`] apply here.
    ///
    /// No Vulkan functions loaded directly or indirectly from this [`Entry`]
    /// may be called after it is [dropped][drop()].
    ///
    /// # Example
    ///
    /// ```no_run
    /// use ash::{vk, Entry};
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let entry = unsafe { Entry::load()? };
    /// let app_info = vk::ApplicationInfo {
    ///     api_version: vk::make_api_version(0, 1, 0, 0),
    ///     ..Default::default()
    /// };
    /// let create_info = vk::InstanceCreateInfo {
    ///     p_application_info: &app_info,
    ///     ..Default::default()
    /// };
    /// let instance = unsafe { entry.create_instance(&create_info, None)? };
    /// # Ok(()) }
    /// ```
    #[cfg(feature = "loaded")]
    #[cfg_attr(docsrs, doc(cfg(feature = "loaded")))]
    pub unsafe fn load() -> Result<Self, LoadingError> {
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

        Self::load_from(LIB_PATH)
    }

    /// Load entry points from a Vulkan loader linked at compile time
    ///
    /// Compared to [`load`](Self::load), this is infallible, but requires that the build
    /// environment have Vulkan development packages installed (e.g. the Vulkan SDK, or Ubuntu's
    /// `libvulkan-dev`), and prevents the resulting binary from starting in environments that do not
    /// support Vulkan.
    ///
    /// Note that instance/device functions are still fetched via `vkGetInstanceProcAddr` and
    /// `vkGetDeviceProcAddr` for maximum performance.
    ///
    /// Any Vulkan function acquired directly or indirectly from this [`Entry`] may be called after it
    /// is [dropped][drop()].
    ///
    /// # Example
    ///
    /// ```no_run
    /// use ash::{vk, Entry};
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let entry = Entry::linked();
    /// let app_info = vk::ApplicationInfo {
    ///     api_version: vk::make_api_version(0, 1, 0, 0),
    ///     ..Default::default()
    /// };
    /// let create_info = vk::InstanceCreateInfo {
    ///     p_application_info: &app_info,
    ///     ..Default::default()
    /// };
    /// let instance = unsafe { entry.create_instance(&create_info, None)? };
    /// # Ok(()) }
    /// ```
    #[cfg(feature = "linked")]
    #[cfg_attr(docsrs, doc(cfg(feature = "linked")))]
    pub fn linked() -> Self {
        // Sound because we're linking to Vulkan, which provides a vkGetInstanceProcAddr that has
        // defined behavior in this use.
        unsafe {
            Self::from_static_fn(vk::StaticFn {
                get_instance_proc_addr: vkGetInstanceProcAddr,
            })
        }
    }

    /// Load Vulkan library at `path`
    ///
    /// # Safety
    ///
    /// `dlopen`ing native libraries is inherently unsafe. The safety guidelines
    /// for [`Library::new()`] and [`Library::get()`] apply here.
    ///
    /// No Vulkan functions loaded directly or indirectly from this [`Entry`]
    /// may be called after it is [dropped][drop()].
    #[cfg(feature = "loaded")]
    #[cfg_attr(docsrs, doc(cfg(feature = "loaded")))]
    pub unsafe fn load_from(path: impl AsRef<OsStr>) -> Result<Self, LoadingError> {
        let lib = Library::new(path)
            .map_err(LoadingError::LibraryLoadFailure)
            .map(Arc::new)?;

        let static_fn = vk::StaticFn::load_checked(|name| {
            lib.get(name.to_bytes_with_nul())
                .map(|symbol| *symbol)
                .unwrap_or(ptr::null_mut())
        })?;

        Ok(Self {
            _lib_guard: Some(lib),
            ..Self::from_static_fn(static_fn)
        })
    }

    /// Load entry points based on an already-loaded [`vk::StaticFn`]
    ///
    /// # Safety
    ///
    /// `static_fn` must contain valid function pointers that comply with the semantics specified
    /// by Vulkan 1.0, which must remain valid for at least the lifetime of the returned [`Entry`].
    pub unsafe fn from_static_fn(static_fn: vk::StaticFn) -> Self {
        let load_fn = |name: &std::ffi::CStr| {
            mem::transmute((static_fn.get_instance_proc_addr)(
                vk::Instance::null(),
                name.as_ptr(),
            ))
        };
        let entry_fn_1_0 = vk::EntryFnV1_0::load(load_fn);
        let entry_fn_1_1 = vk::EntryFnV1_1::load(load_fn);
        let entry_fn_1_2 = vk::EntryFnV1_2::load(load_fn);
        let entry_fn_1_3 = vk::EntryFnV1_3::load(load_fn);

        Self {
            static_fn,
            entry_fn_1_0,
            entry_fn_1_1,
            entry_fn_1_2,
            entry_fn_1_3,
            #[cfg(feature = "loaded")]
            _lib_guard: None,
        }
    }

    #[inline]
    pub fn fp_v1_0(&self) -> &vk::EntryFnV1_0 {
        &self.entry_fn_1_0
    }

    #[inline]
    pub fn static_fn(&self) -> &vk::StaticFn {
        &self.static_fn
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateInstanceVersion.html>
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use ash::{Entry, vk};
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let entry = Entry::linked();
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
    #[inline]
    pub fn try_enumerate_instance_version(&self) -> VkResult<Option<u32>> {
        unsafe {
            let mut api_version = 0;
            let enumerate_instance_version: Option<vk::PFN_vkEnumerateInstanceVersion> = {
                let name = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateInstanceVersion\0",
                );
                mem::transmute((self.static_fn.get_instance_proc_addr)(
                    vk::Instance::null(),
                    name.as_ptr(),
                ))
            };
            if let Some(enumerate_instance_version) = enumerate_instance_version {
                (enumerate_instance_version)(&mut api_version)
                    .result_with_success(Some(api_version))
            } else {
                Ok(None)
            }
        }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateInstance.html>
    ///
    /// # Safety
    ///
    /// The resulting [`Instance`] and any function-pointer objects (e.g. [`Device`][crate::Device]
    /// and [extensions][crate::extensions]) loaded from it may not be used after this [`Entry`]
    /// object is dropped, unless it was crated using [`Entry::linked()`].
    ///
    /// [`Instance`] does _not_ implement [drop][drop()] semantics and can only be destroyed via
    /// [`destroy_instance()`][Instance::destroy_instance()].
    #[inline]
    pub unsafe fn create_instance(
        &self,
        create_info: &vk::InstanceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Instance> {
        let mut instance = mem::zeroed();
        (self.entry_fn_1_0.create_instance)(
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut instance,
        )
        .result()?;
        Ok(Instance::load(&self.static_fn, instance))
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateInstanceLayerProperties.html>
    #[inline]
    pub fn enumerate_instance_layer_properties(&self) -> VkResult<Vec<vk::LayerProperties>> {
        unsafe {
            read_into_uninitialized_vector(|count, data| {
                (self.entry_fn_1_0.enumerate_instance_layer_properties)(count, data)
            })
        }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateInstanceExtensionProperties.html>
    #[inline]
    pub fn enumerate_instance_extension_properties(
        &self,
        layer_name: Option<&CStr>,
    ) -> VkResult<Vec<vk::ExtensionProperties>> {
        unsafe {
            read_into_uninitialized_vector(|count, data| {
                (self.entry_fn_1_0.enumerate_instance_extension_properties)(
                    layer_name.map_or(ptr::null(), |str| str.as_ptr()),
                    count,
                    data,
                )
            })
        }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetInstanceProcAddr.html>
    #[inline]
    pub unsafe fn get_instance_proc_addr(
        &self,
        instance: vk::Instance,
        p_name: *const c_char,
    ) -> vk::PFN_vkVoidFunction {
        (self.static_fn.get_instance_proc_addr)(instance, p_name)
    }
}

/// Vulkan core 1.1
#[allow(non_camel_case_types)]
impl Entry {
    #[inline]
    pub fn fp_v1_1(&self) -> &vk::EntryFnV1_1 {
        &self.entry_fn_1_1
    }

    #[deprecated = "This function is unavailable and therefore panics on Vulkan 1.0, please use `try_enumerate_instance_version()` instead"]
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateInstanceVersion.html>
    ///
    /// Please use [`try_enumerate_instance_version()`][Self::try_enumerate_instance_version()] instead.
    #[inline]
    pub fn enumerate_instance_version(&self) -> VkResult<u32> {
        unsafe {
            let mut api_version = 0;
            (self.entry_fn_1_1.enumerate_instance_version)(&mut api_version)
                .result_with_success(api_version)
        }
    }
}

/// Vulkan core 1.2
#[allow(non_camel_case_types)]
impl Entry {
    #[inline]
    pub fn fp_v1_2(&self) -> &vk::EntryFnV1_2 {
        &self.entry_fn_1_2
    }
}

/// Vulkan core 1.3
#[allow(non_camel_case_types)]
impl Entry {
    #[inline]
    pub fn fp_v1_3(&self) -> &vk::EntryFnV1_3 {
        &self.entry_fn_1_3
    }
}

#[cfg(feature = "linked")]
#[cfg_attr(docsrs, doc(cfg(feature = "linked")))]
impl Default for Entry {
    #[inline]
    fn default() -> Self {
        Self::linked()
    }
}

impl vk::StaticFn {
    pub fn load_checked<F>(mut _f: F) -> Result<Self, MissingEntryPoint>
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        // TODO: Make this a &'static CStr once CStr::from_bytes_with_nul_unchecked is const
        static ENTRY_POINT: &[u8] = b"vkGetInstanceProcAddr\0";

        Ok(Self {
            get_instance_proc_addr: unsafe {
                let cname = CStr::from_bytes_with_nul_unchecked(ENTRY_POINT);
                let val = _f(cname);
                if val.is_null() {
                    return Err(MissingEntryPoint);
                } else {
                    ::std::mem::transmute(val)
                }
            },
        })
    }
}

#[derive(Clone, Debug)]
pub struct MissingEntryPoint;
impl std::fmt::Display for MissingEntryPoint {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::result::Result<(), std::fmt::Error> {
        write!(f, "Cannot load `vkGetInstanceProcAddr` symbol from library")
    }
}
impl std::error::Error for MissingEntryPoint {}

#[cfg(feature = "linked")]
extern "system" {
    fn vkGetInstanceProcAddr(instance: vk::Instance, name: *const c_char)
        -> vk::PFN_vkVoidFunction;
}

#[cfg(feature = "loaded")]
mod loaded {
    use std::error::Error;
    use std::fmt;

    use super::*;

    #[derive(Debug)]
    #[cfg_attr(docsrs, doc(cfg(feature = "loaded")))]
    pub enum LoadingError {
        LibraryLoadFailure(libloading::Error),
        MissingEntryPoint(MissingEntryPoint),
    }

    impl fmt::Display for LoadingError {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            match self {
                Self::LibraryLoadFailure(err) => fmt::Display::fmt(err, f),
                Self::MissingEntryPoint(err) => fmt::Display::fmt(err, f),
            }
        }
    }

    impl Error for LoadingError {
        fn source(&self) -> Option<&(dyn Error + 'static)> {
            Some(match self {
                Self::LibraryLoadFailure(err) => err,
                Self::MissingEntryPoint(err) => err,
            })
        }
    }

    impl From<MissingEntryPoint> for LoadingError {
        fn from(err: MissingEntryPoint) -> Self {
            Self::MissingEntryPoint(err)
        }
    }
}
#[cfg(feature = "loaded")]
pub use self::loaded::*;
