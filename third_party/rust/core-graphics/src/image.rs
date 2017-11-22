use core_foundation::base::{CFRetain, CFTypeID};
use core_foundation::data::CFData;
use color_space::CGColorSpace;
use data_provider::CGDataProviderRef;
use libc::size_t;
use foreign_types::{ForeignType, ForeignTypeRef};

#[repr(C)]
pub enum CGImageAlphaInfo {
    CGImageAlphaNone, /* For example, RGB. */
    CGImageAlphaPremultipliedLast, /* For example, premultiplied RGBA */
    CGImageAlphaPremultipliedFirst, /* For example, premultiplied ARGB */
    CGImageAlphaLast, /* For example, non-premultiplied RGBA */
    CGImageAlphaFirst, /* For example, non-premultiplied ARGB */
    CGImageAlphaNoneSkipLast, /* For example, RBGX. */
    CGImageAlphaNoneSkipFirst, /* For example, XRBG. */
    CGImageAlphaOnly /* No color data, alpha data only */
}

#[repr(C)]
pub enum CGImageByteOrderInfo {
    CGImageByteOrderMask = 0x7000,
    CGImageByteOrder16Little = (1 << 12),
    CGImageByteOrder32Little = (2 << 12),
    CGImageByteOrder16Big = (3 << 12),
    CGImageByteOrder32Big = (4 << 12)
}

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGImage;
    fn drop = CGImageRelease;
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGImage;
    pub struct CGImageRef;
}

impl CGImage {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGImageGetTypeID()
        }
    }
}

impl CGImageRef {
    pub fn width(&self) -> size_t {
        unsafe {
            CGImageGetWidth(self.as_ptr())
        }
    }

    pub fn height(&self) -> size_t {
        unsafe {
            CGImageGetHeight(self.as_ptr())
        }
    }

    pub fn bits_per_component(&self) -> size_t {
        unsafe {
            CGImageGetBitsPerComponent(self.as_ptr())
        }
    }

    pub fn bits_per_pixel(&self) -> size_t {
        unsafe {
            CGImageGetBitsPerPixel(self.as_ptr())
        }
    }

    pub fn bytes_per_row(&self) -> size_t {
        unsafe {
            CGImageGetBytesPerRow(self.as_ptr())
        }
    }

    pub fn color_space(&self) -> CGColorSpace {
        unsafe {
            let cs = CGImageGetColorSpace(self.as_ptr());
            CFRetain(cs as *mut _);
            CGColorSpace::from_ptr(cs)
        }
    }

    /// Returns the raw image bytes wrapped in `CFData`. Note, the returned `CFData` owns the
    /// underlying buffer.
    pub fn data(&self) -> CFData {
        let data_provider = unsafe {
            CGDataProviderRef::from_ptr(CGImageGetDataProvider(self.as_ptr()))
        };
        data_provider.copy_data()
    }
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    fn CGImageGetTypeID() -> CFTypeID;
    fn CGImageGetWidth(image: ::sys::CGImageRef) -> size_t;
    fn CGImageGetHeight(image: ::sys::CGImageRef) -> size_t;
    fn CGImageGetBitsPerComponent(image: ::sys::CGImageRef) -> size_t;
    fn CGImageGetBitsPerPixel(image: ::sys::CGImageRef) -> size_t;
    fn CGImageGetBytesPerRow(image: ::sys::CGImageRef) -> size_t;
    fn CGImageGetColorSpace(image: ::sys::CGImageRef) -> ::sys::CGColorSpaceRef;
    fn CGImageGetDataProvider(image: ::sys::CGImageRef) -> ::sys::CGDataProviderRef;
    fn CGImageRelease(image: ::sys::CGImageRef);

    //fn CGImageGetAlphaInfo(image: ::sys::CGImageRef) -> CGImageAlphaInfo;
    //fn CGImageCreateCopyWithColorSpace(image: ::sys::CGImageRef, space: ::sys::CGColorSpaceRef) -> ::sys::CGImageRef
}
