
#[cfg(any(target_os = "linux",
          target_os = "android",
          target_os = "macos",
          target_os = "ios",
          target_os = "bitrig",
          target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "netbsd",
          target_os = "openbsd",
          target_os = "solaris"))]
mod unix;
#[cfg(any(target_os = "linux",
          target_os = "android",
          target_os = "macos",
          target_os = "ios",
          target_os = "bitrig",
          target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "netbsd",
          target_os = "openbsd",
          target_os = "solaris"))]
pub use self::unix::{dimensions, dimensions_stdout, dimensions_stdin, dimensions_stderr};

#[cfg(target_os = "windows")]
mod windows;
#[cfg(target_os = "windows")]
pub use self::windows::{dimensions, dimensions_stdout, dimensions_stdin, dimensions_stderr};

// makes project compilable on unsupported platforms
#[cfg(not(any(target_os = "linux",
              target_os = "android",
              target_os = "macos",
              target_os = "ios",
              target_os = "bitrig",
              target_os = "dragonfly",
              target_os = "freebsd",
              target_os = "netbsd",
              target_os = "openbsd",
              target_os = "solaris",
              target_os = "windows")))]
mod unsupported;
#[cfg(not(any(target_os = "linux",
              target_os = "android",
              target_os = "macos",
              target_os = "ios",
              target_os = "bitrig",
              target_os = "dragonfly",
              target_os = "freebsd",
              target_os = "netbsd",
              target_os = "openbsd",
              target_os = "solaris",
              target_os = "windows")))]
pub use self::unsupported::{dimensions, dimensions_stdout, dimensions_stdin, dimensions_stderr};