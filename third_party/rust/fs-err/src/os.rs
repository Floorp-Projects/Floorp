//! OS-specific functionality.

// The std-library has a couple more platforms than just `unix` for which these apis
// are defined, but we're using just `unix` here. We can always expand later.
#[cfg(unix)]
/// Platform-specific extensions for Unix platforms.
pub mod unix;

#[cfg(windows)]
/// Platform-specific extensions for Windows.
pub mod windows;
