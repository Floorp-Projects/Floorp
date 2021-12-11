use crate::entry::EntryCustom;
use crate::entry::MissingEntryPoint;
use libloading::Library;
use std::error::Error;
use std::ffi::OsStr;
use std::fmt;
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

#[derive(Debug)]
pub enum LoadingError {
    LibraryLoadFailure(libloading::Error),
    MissingEntryPoint(MissingEntryPoint),
}

impl fmt::Display for LoadingError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            LoadingError::LibraryLoadFailure(err) => fmt::Display::fmt(err, f),
            LoadingError::MissingEntryPoint(err) => fmt::Display::fmt(err, f),
        }
    }
}

impl Error for LoadingError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        Some(match self {
            LoadingError::LibraryLoadFailure(err) => err,
            LoadingError::MissingEntryPoint(err) => err,
        })
    }
}

impl From<MissingEntryPoint> for LoadingError {
    fn from(err: MissingEntryPoint) -> Self {
        LoadingError::MissingEntryPoint(err)
    }
}

/// Default function loader
pub type Entry = EntryCustom<Arc<Library>>;

impl Entry {
    /// Load default Vulkan library for the current platform
    ///
    /// # Safety
    /// `dlopen`ing native libraries is inherently unsafe. The safety guidelines
    /// for [`Library::new`] and [`Library::get`] apply here.
    ///
    /// ```no_run
    /// use ash::{vk, Entry};
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let entry = unsafe { Entry::new() }?;
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
    pub unsafe fn new() -> Result<Entry, LoadingError> {
        Self::with_library(LIB_PATH)
    }

    /// Load Vulkan library at `path`
    ///
    /// # Safety
    /// `dlopen`ing native libraries is inherently unsafe. The safety guidelines
    /// for [`Library::new`] and [`Library::get`] apply here.
    pub unsafe fn with_library(path: impl AsRef<OsStr>) -> Result<Entry, LoadingError> {
        let lib = Library::new(path)
            .map_err(LoadingError::LibraryLoadFailure)
            .map(Arc::new)?;

        Ok(Self::new_custom(lib, |vk_lib, name| {
            vk_lib
                .get(name.to_bytes_with_nul())
                .map(|symbol| *symbol)
                .unwrap_or(ptr::null_mut())
        })?)
    }
}
