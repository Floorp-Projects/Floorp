// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::ops;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CorkState(u32);

const UNCORK: u32 = 0b00;
const CORK: u32 = 0b01;
const NOTIFY: u32 = 0b10;
const ALL: u32 = 0b11;

impl CorkState {
    #[inline]
    pub fn uncork() -> Self {
        CorkState(UNCORK)
    }
    #[inline]
    pub fn cork() -> Self {
        CorkState(CORK)
    }
    #[inline]
    pub fn notify() -> Self {
        CorkState(NOTIFY)
    }

    #[inline]
    pub fn is_cork(&self) -> bool {
        self.contains(CorkState::cork())
    }
    #[inline]
    pub fn is_notify(&self) -> bool {
        self.contains(CorkState::notify())
    }

    #[inline]
    pub fn contains(&self, other: Self) -> bool {
        (*self & other) == other
    }
}

impl ops::BitOr for CorkState {
    type Output = CorkState;

    #[inline]
    fn bitor(self, other: Self) -> Self {
        CorkState(self.0 | other.0)
    }
}

impl ops::BitXor for CorkState {
    type Output = CorkState;

    #[inline]
    fn bitxor(self, other: Self) -> Self {
        CorkState(self.0 ^ other.0)
    }
}

impl ops::BitAnd for CorkState {
    type Output = CorkState;

    #[inline]
    fn bitand(self, other: Self) -> Self {
        CorkState(self.0 & other.0)
    }
}

impl ops::Sub for CorkState {
    type Output = CorkState;

    #[inline]
    fn sub(self, other: Self) -> Self {
        CorkState(self.0 & !other.0)
    }
}

impl ops::Not for CorkState {
    type Output = CorkState;

    #[inline]
    fn not(self) -> Self {
        CorkState(!self.0 & ALL)
    }
}
