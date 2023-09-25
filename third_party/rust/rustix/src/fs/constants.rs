//! Filesystem API constants, translated into `bitflags` constants.

use crate::backend;

pub use crate::io::FdFlags;
#[cfg(not(target_os = "espidf"))]
pub use backend::fs::types::Access;
pub use backend::fs::types::{Dev, Mode, OFlags};

#[cfg(not(any(target_os = "espidf", target_os = "redox")))]
pub use backend::fs::types::AtFlags;

#[cfg(apple)]
pub use backend::fs::types::{CloneFlags, CopyfileFlags};

#[cfg(linux_kernel)]
pub use backend::fs::types::*;

pub use crate::timespec::{Nsecs, Secs, Timespec};
