pub enum CGImage {}
pub type CGImageRef = *mut CGImage;

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
