extern crate libudev_sys as ffi;
extern crate libc;

pub use context::{Context};
pub use device::{Device,Properties,Property,Attributes,Attribute};
pub use enumerator::{Enumerator,Devices};
pub use error::{Result,Error,ErrorKind};
pub use monitor::{Monitor,MonitorSocket,EventType,Event};

macro_rules! try_alloc {
    ($exp:expr) => {{
        let ptr = $exp;

        if ptr.is_null() {
            return Err(::error::from_errno(::libc::ENOMEM));
        }

        ptr
    }}
}

mod context;
mod device;
mod enumerator;
mod error;
mod monitor;

mod handle;
mod util;
