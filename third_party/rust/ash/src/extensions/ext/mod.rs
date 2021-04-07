#[allow(deprecated)]
pub use self::debug_marker::DebugMarker;
#[allow(deprecated)]
pub use self::debug_report::DebugReport;
pub use self::debug_utils::DebugUtils;
pub use self::metal_surface::MetalSurface;
pub use self::tooling_info::ToolingInfo;

#[deprecated(note = "Please use the [DebugUtils](struct.DebugUtils.html) extension instead.")]
mod debug_marker;
#[deprecated(note = "Please use the [DebugUtils](struct.DebugUtils.html) extension instead.")]
mod debug_report;
mod debug_utils;
mod metal_surface;
mod tooling_info;
