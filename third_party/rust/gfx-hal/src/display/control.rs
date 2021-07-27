//! Display power control structures.

/// Error occurring while controlling the display power.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum DisplayControlError {
    /// Host memory exhausted.
    #[error("Out of host memory")]
    OutOfHostMemory,
    /// The feature is unsupported by the device
    #[error("The feature is unsupported by the device")]
    UnsupportedFeature,
}

/// Available power states of a display.
#[derive(Clone, Debug, PartialEq)]
pub enum PowerState {
    /// Specifies that the display is powered down
    Off,
    /// Specifies that the display is put into a low power mode, from which it may be able to transition back to PowerState::On more quickly than if it were in PowerState::Off.
    /// This state may be the same as PowerState::Off
    Suspend,
    /// Specifies that the display is powered on.
    On,
}

/// Device event
#[derive(Clone, Debug, PartialEq)]
pub enum DeviceEvent {
    /// Specifies that the fence is signaled when a display is plugged into or unplugged from the specified device.
    /// Applications can use this notification to determine when they need to re-enumerate the available displays on a device.
    DisplayHotplug,
}

/// Device event
#[derive(Clone, Debug, PartialEq)]
pub enum DisplayEvent {
    /// Specifies that the fence is signaled when the first pixel of the next display refresh cycle leaves the display engine for the display.
    FirstPixelOut,
}
