// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod os {
    #[cfg(target_os = "linux")]
    include!("linux/mod.rs");

    #[cfg(target_os = "macos")]
    include!("macos/mod.rs");

    #[cfg(any(target_os = "windows", target_os = "android"))]
    include!("inprocess/mod.rs");
}

pub use self::os::{OsIpcChannel, OsIpcOneShotServer, OsIpcReceiver, OsIpcReceiverSet};
pub use self::os::{OsIpcSelectionResult, OsIpcSender, OsIpcSharedMemory};
pub use self::os::{OsOpaqueIpcChannel, channel};

#[cfg(test)]
mod test;
