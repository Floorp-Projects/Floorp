/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::*;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct PLArena {
    pub next: *mut PLArena,
    pub base: PRUword,
    pub limit: PRUword,
    pub avail: PRUword,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct PLArenaPool {
    pub first: PLArena,
    pub current: *mut PLArena,
    pub arenasize: PRUint32,
    pub mask: PRUword,
}
