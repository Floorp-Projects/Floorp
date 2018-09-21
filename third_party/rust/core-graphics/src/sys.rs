use std::os::raw::c_void;

pub enum CGImage {}
pub type CGImageRef = *mut CGImage;

#[repr(C)]
pub struct __CGColor(c_void);

pub type CGColorRef = *const __CGColor;

pub enum CGColorSpace {}
pub type CGColorSpaceRef = *mut CGColorSpace;

pub enum CGPath {}
pub type CGPathRef = *mut CGPath;

pub enum CGDataProvider {}
pub type CGDataProviderRef = *mut CGDataProvider;

pub enum CGFont {}
pub type CGFontRef = *mut CGFont;

pub enum CGContext {}
pub type CGContextRef = *mut CGContext;

#[cfg(target_os = "macos")]
mod macos {
	pub enum CGEvent {}
	pub type CGEventRef = *mut CGEvent;

	pub enum CGEventSource {}
	pub type CGEventSourceRef = *mut CGEventSource;

	pub enum CGDisplayMode {}
	pub type CGDisplayModeRef = *mut CGDisplayMode;
}

#[cfg(target_os = "macos")]
pub use self::macos::*;
