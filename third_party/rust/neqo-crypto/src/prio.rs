// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(
    dead_code,
    non_upper_case_globals,
    non_snake_case,
    clippy::cognitive_complexity,
    clippy::empty_enum,
    clippy::too_many_lines,
    clippy::upper_case_acronyms
)]
#![allow(unknown_lints, renamed_and_removed_lints, clippy::unknown_clippy_lints)] // Until we require rust 1.51.

include!(concat!(env!("OUT_DIR"), "/nspr_io.rs"));

pub enum PRFileInfo {}
pub enum PRFileInfo64 {}
pub enum PRFilePrivate {}
pub enum PRIOVec {}
pub enum PRSendFileData {}
