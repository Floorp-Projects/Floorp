// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(non_snake_case)]
#![allow(clippy::cognitive_complexity)]
#![allow(clippy::empty_enum)]

include!(concat!(env!("OUT_DIR"), "/nspr_io.rs"));

pub enum PRFileInfo {}
pub enum PRFileInfo64 {}
pub enum PRFilePrivate {}
pub enum PRIOVec {}
pub enum PRSendFileData {}
