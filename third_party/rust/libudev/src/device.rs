use std::str;

use std::ffi::{CStr,OsStr};
use std::path::Path;
use std::str::FromStr;

use libc::{c_char,dev_t};

use ::context::Context;
use ::handle::*;

pub fn new(context: &Context, device: *mut ::ffi::udev_device) -> Device {
    Device {
        _context: context,
        device: device,
    }
}

/// A structure that provides access to sysfs/kernel devices.
pub struct Device<'a> {
    _context: &'a Context,
    device: *mut ::ffi::udev_device,
}

impl<'a> Drop for Device<'a> {
    fn drop(&mut self) {
        unsafe {
            ::ffi::udev_device_unref(self.device);
        }
    }
}

#[doc(hidden)]
impl<'a> Handle<::ffi::udev_device> for Device<'a> {
    fn as_ptr(&self) -> *mut ::ffi::udev_device {
        self.device
    }
}

impl<'a> Device<'a> {
    /// Checks whether the device has already been handled by udev.
    ///
    /// When a new device is connected to the system, udev initializes the device by setting
    /// permissions, renaming network devices, and possibly other initialization routines. This
    /// method returns `true` if udev has performed all of its work to initialize this device.
    ///
    /// This method only applies to devices with device nodes or network interfaces. All other
    /// devices return `true` by default.
    pub fn is_initialized(&self) -> bool {
        unsafe {
            ::ffi::udev_device_get_is_initialized(self.device) > 0
        }
    }

    /// Gets the device's major/minor number.
    pub fn devnum(&self) -> Option<dev_t> {
        match unsafe { ::ffi::udev_device_get_devnum(self.device) } {
            0 => None,
            n => Some(n)
        }
    }

    /// Returns the syspath of the device.
    ///
    /// The path is an absolute path and includes the sys mount point. For example, the syspath for
    /// `tty0` could be `/sys/devices/virtual/tty/tty0`, which includes the sys mount point,
    /// `/sys`.
    pub fn syspath(&self) -> &Path {
        Path::new(unsafe {
            ::util::ptr_to_os_str_unchecked(::ffi::udev_device_get_syspath(self.device))
        })
    }

    /// Returns the kernel devpath value of the device.
    ///
    /// The path does not contain the sys mount point, but does start with a `/`. For example, the
    /// devpath for `tty0` could be `/devices/virtual/tty/tty0`.
    pub fn devpath(&self) -> &OsStr {
        unsafe {
            ::util::ptr_to_os_str_unchecked(::ffi::udev_device_get_devpath(self.device))
        }
    }

    /// Returns the path to the device node belonging to the device.
    ///
    /// The path is an absolute path and starts with the device directory. For example, the device
    /// node for `tty0` could be `/dev/tty0`.
    pub fn devnode(&self) -> Option<&Path> {
        ::util::ptr_to_os_str(unsafe { ::ffi::udev_device_get_devnode(self.device) }).map(|path| {
            Path::new(path)
        })
    }

    /// Returns the parent of the device.
    pub fn parent(&self) -> Option<Device> {
        let ptr = unsafe { ::ffi::udev_device_get_parent(self.device) };

        if !ptr.is_null() {
            unsafe {
                ::ffi::udev_device_ref(ptr);
            }

            Some(Device {
                _context: self._context,
                device: ptr,
            })
        }
        else {
            None
        }
    }

    /// Returns the subsystem name of the device.
    ///
    /// The subsystem name is a string that indicates which kernel subsystem the device belongs to.
    /// Examples of subsystem names are `tty`, `vtconsole`, `block`, `scsi`, and `net`.
    pub fn subsystem(&self) -> &OsStr {
        unsafe {
            ::util::ptr_to_os_str_unchecked(::ffi::udev_device_get_subsystem(self.device))
        }
    }

    /// Returns the kernel device name for the device.
    ///
    /// The sysname is a string that differentiates the device from others in the same subsystem.
    /// For example, `tty0` is the sysname for a TTY device that differentiates it from others,
    /// such as `tty1`.
    pub fn sysname(&self) -> &OsStr {
        unsafe {
            ::util::ptr_to_os_str_unchecked(::ffi::udev_device_get_sysname(self.device))
        }
    }

    /// Returns the instance number of the device.
    ///
    /// The instance number is used to differentiate many devices of the same type. For example,
    /// `/dev/tty0` and `/dev/tty1` are both TTY devices but have instance numbers of 0 and 1,
    /// respectively.
    ///
    /// Some devices don't have instance numbers, such as `/dev/console`, in which case the method
    /// returns `None`.
    pub fn sysnum(&self) -> Option<usize> {
        let ptr = unsafe { ::ffi::udev_device_get_sysnum(self.device) };

        if !ptr.is_null() {
            match str::from_utf8(unsafe { CStr::from_ptr(ptr) }.to_bytes()) {
                Err(_) => None,
                Ok(s) => FromStr::from_str(s).ok()
            }
        }
        else {
            None
        }
    }

    /// Returns the devtype name of the device.
    pub fn devtype(&self) -> Option<&OsStr> {
        ::util::ptr_to_os_str(unsafe { ::ffi::udev_device_get_devtype(self.device) })
    }

    /// Returns the name of the kernel driver attached to the device.
    pub fn driver(&self) -> Option<&OsStr> {
        ::util::ptr_to_os_str(unsafe { ::ffi::udev_device_get_driver(self.device) })
    }

    /// Retreives the value of a device property.
    pub fn property_value<T: AsRef<OsStr>>(&self, property: T) -> Option<&OsStr> {
        let prop = match ::util::os_str_to_cstring(property) {
            Ok(s) => s,
            Err(_) => return None
        };

        ::util::ptr_to_os_str(unsafe {
            ::ffi::udev_device_get_property_value(self.device, prop.as_ptr())
        })
    }

    /// Retreives the value of a device attribute.
    pub fn attribute_value<T: AsRef<OsStr>>(&self, attribute: T) -> Option<&OsStr> {
        let attr = match ::util::os_str_to_cstring(attribute) {
            Ok(s) => s,
            Err(_) => return None
        };

        ::util::ptr_to_os_str(unsafe {
            ::ffi::udev_device_get_sysattr_value(self.device, attr.as_ptr())
        })
    }

    /// Sets the value of a device attribute.
    pub fn set_attribute_value<T: AsRef<OsStr>, U: AsRef<OsStr>>(&mut self, attribute: T, value: U) -> ::Result<()> {
        let attribute = try!(::util::os_str_to_cstring(attribute));
        let value = try!(::util::os_str_to_cstring(value));

        ::util::errno_to_result(unsafe {
            ::ffi::udev_device_set_sysattr_value(self.device, attribute.as_ptr(), value.as_ptr() as *mut c_char)
        })
    }

    /// Returns an iterator over the device's properties.
    ///
    /// ## Example
    ///
    /// This example prints out all of a device's properties:
    ///
    /// ```no_run
    /// # use std::path::Path;
    /// # let mut context = libudev::Context::new().unwrap();
    /// # let device = context.device_from_syspath(Path::new("/sys/devices/virtual/tty/tty0")).unwrap();
    /// for property in device.properties() {
    ///     println!("{:?} = {:?}", property.name(), property.value());
    /// }
    /// ```
    pub fn properties(&self) -> Properties {
        Properties {
            _device: self,
            entry: unsafe { ::ffi::udev_device_get_properties_list_entry(self.device) }
        }
    }

    /// Returns an iterator over the device's attributes.
    ///
    /// ## Example
    ///
    /// This example prints out all of a device's attributes:
    ///
    /// ```no_run
    /// # use std::path::Path;
    /// # let mut context = libudev::Context::new().unwrap();
    /// # let device = context.device_from_syspath(Path::new("/sys/devices/virtual/tty/tty0")).unwrap();
    /// for attribute in device.attributes() {
    ///     println!("{:?} = {:?}", attribute.name(), attribute.value());
    /// }
    /// ```
    pub fn attributes(&self) -> Attributes {
        Attributes {
            device: self,
            entry: unsafe { ::ffi::udev_device_get_sysattr_list_entry(self.device) }
        }
    }
}


/// Iterator over a device's properties.
pub struct Properties<'a> {
    _device: &'a Device<'a>,
    entry: *mut ::ffi::udev_list_entry
}

impl<'a> Iterator for Properties<'a> {
    type Item = Property<'a>;

    fn next(&mut self) -> Option<Property<'a>> {
        if self.entry.is_null() {
            None
        }
        else {
            let name = unsafe { ::util::ptr_to_os_str_unchecked(::ffi::udev_list_entry_get_name(self.entry)) };
            let value = unsafe { ::util::ptr_to_os_str_unchecked(::ffi::udev_list_entry_get_value(self.entry)) };

            self.entry = unsafe { ::ffi::udev_list_entry_get_next(self.entry) };

            Some(Property {
                name: name,
                value: value
            })
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, None)
    }
}

/// A device property.
pub struct Property<'a> {
    name: &'a OsStr,
    value: &'a OsStr
}

impl<'a> Property<'a> {
    /// Returns the property name.
    pub fn name(&self) -> &OsStr {
        self.name
    }

    /// Returns the property value.
    pub fn value(&self) -> &OsStr {
        self.value
    }
}


/// Iterator over a device's attributes.
pub struct Attributes<'a> {
    device: &'a Device<'a>,
    entry: *mut ::ffi::udev_list_entry
}

impl<'a> Iterator for Attributes<'a> {
    type Item = Attribute<'a>;

    fn next(&mut self) -> Option<Attribute<'a>> {
        if !self.entry.is_null() {
            let name = unsafe { ::util::ptr_to_os_str_unchecked(::ffi::udev_list_entry_get_name(self.entry)) };

            self.entry = unsafe { ::ffi::udev_list_entry_get_next(self.entry) };

            Some(Attribute {
                device: self.device,
                name: name
            })
        }
        else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, None)
    }
}

/// A device attribute.
pub struct Attribute<'a> {
    device: &'a Device<'a>,
    name: &'a OsStr
}

impl<'a> Attribute<'a> {
    /// Returns the attribute name.
    pub fn name(&self) -> &OsStr {
        self.name
    }

    /// Returns the attribute value.
    pub fn value(&self) -> Option<&OsStr> {
        self.device.attribute_value(self.name)
    }
}
