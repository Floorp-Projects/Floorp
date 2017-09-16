use std::ffi::{OsStr};
use std::path::Path;

use ::handle::prelude::*;

pub use context::{Context};
pub use device::{Device};


/// An enumeration context.
///
/// An Enumerator scans `/sys` for devices matching its filters. Filters are added to an Enumerator
/// by calling its `match_*` and `nomatch_*` methods. After the filters are setup, the
/// `scan_devices()` method finds devices in `/sys` that match the filters.
pub struct Enumerator<'a> {
    context: &'a Context,
    enumerator: *mut ::ffi::udev_enumerate
}

impl<'a> Drop for Enumerator<'a> {
    fn drop(&mut self) {
        unsafe { ::ffi::udev_enumerate_unref(self.enumerator) };
    }
}

impl<'a> Enumerator<'a> {
    /// Creates a new Enumerator.
    pub fn new(context: &'a Context) -> ::Result<Self> {
        let ptr = try_alloc!(unsafe { ::ffi::udev_enumerate_new(context.as_ptr()) });

        Ok(Enumerator {
            context: context,
            enumerator: ptr
        })
    }

    /// Adds a filter that matches only initialized devices.
    pub fn match_is_initialized(&mut self) -> ::Result<()> {
        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_is_initialized(self.enumerator)
        })
    }

    /// Adds a filter that matches only devices that belong to the given kernel subsystem.
    pub fn match_subsystem<T: AsRef<OsStr>>(&mut self, subsystem: T) -> ::Result<()> {
        let subsystem = try!(::util::os_str_to_cstring(subsystem));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_subsystem(self.enumerator, subsystem.as_ptr())
        })
    }

    /// Adds a filter that matches only devices with the given attribute value.
    pub fn match_attribute<T: AsRef<OsStr>, U: AsRef<OsStr>>(&mut self, attribute: T, value: U) -> ::Result<()> {
        let attribute = try!(::util::os_str_to_cstring(attribute));
        let value = try!(::util::os_str_to_cstring(value));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_sysattr(self.enumerator, attribute.as_ptr(), value.as_ptr())
        })
    }

    /// Adds a filter that matches only devices with the given kernel device name.
    pub fn match_sysname<T: AsRef<OsStr>>(&mut self, sysname: T) -> ::Result<()> {
        let sysname = try!(::util::os_str_to_cstring(sysname));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_sysname(self.enumerator, sysname.as_ptr())
        })
    }

    /// Adds a filter that matches only devices with the given property value.
    pub fn match_property<T: AsRef<OsStr>, U: AsRef<OsStr>>(&mut self, property: T, value: U) -> ::Result<()> {
        let property = try!(::util::os_str_to_cstring(property));
        let value = try!(::util::os_str_to_cstring(value));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_property(self.enumerator, property.as_ptr(), value.as_ptr())
        })
    }

    /// Adds a filter that matches only devices with the given tag.
    pub fn match_tag<T: AsRef<OsStr>>(&mut self, tag: T) -> ::Result<()> {
        let tag = try!(::util::os_str_to_cstring(tag));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_tag(self.enumerator, tag.as_ptr())
        })
    }

    /// Includes the parent device and all devices in the subtree of the parent device.
    pub fn match_parent(&mut self, parent: &Device) -> ::Result<()> {
        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_match_parent(self.enumerator, parent.as_ptr())
        })
    }

    /// Adds a filter that matches only devices that don't belong to the given kernel subsystem.
    pub fn nomatch_subsystem<T: AsRef<OsStr>>(&mut self, subsystem: T) -> ::Result<()> {
        let subsystem = try!(::util::os_str_to_cstring(subsystem));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_nomatch_subsystem(self.enumerator, subsystem.as_ptr())
        })
    }

    /// Adds a filter that matches only devices that don't have the the given attribute value.
    pub fn nomatch_attribute<T: AsRef<OsStr>, U: AsRef<OsStr>>(&mut self, attribute: T, value: U) -> ::Result<()> {
        let attribute = try!(::util::os_str_to_cstring(attribute));
        let value = try!(::util::os_str_to_cstring(value));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_nomatch_sysattr(self.enumerator, attribute.as_ptr(), value.as_ptr())
        })
    }

    /// Includes the device with the given syspath.
    pub fn add_syspath(&mut self, syspath: &Path) -> ::Result<()> {
        let syspath = try!(::util::os_str_to_cstring(syspath));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_add_syspath(self.enumerator, syspath.as_ptr())
        })
    }

    /// Scans `/sys` for devices matching the attached filters.
    ///
    /// The devices will be sorted in dependency order.
    pub fn scan_devices(&mut self) -> ::Result<Devices> {
        try!(::util::errno_to_result(unsafe {
            ::ffi::udev_enumerate_scan_devices(self.enumerator)
        }));

        Ok(Devices {
            enumerator: self,
            entry: unsafe { ::ffi::udev_enumerate_get_list_entry(self.enumerator) }
        })
    }
}


/// Iterator over devices.
pub struct Devices<'a> {
    enumerator: &'a Enumerator<'a>,
    entry: *mut ::ffi::udev_list_entry
}

impl<'a> Iterator for Devices<'a> {
    type Item = Device<'a>;

    fn next(&mut self) -> Option<Device<'a>> {
        while !self.entry.is_null() {
            let syspath = Path::new(unsafe {
                ::util::ptr_to_os_str_unchecked(::ffi::udev_list_entry_get_name(self.entry))
            });

            self.entry = unsafe { ::ffi::udev_list_entry_get_next(self.entry) };

            match self.enumerator.context.device_from_syspath(syspath) {
                Ok(d) => return Some(d),
                Err(_) => continue
            };
        }

        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, None)
    }
}
