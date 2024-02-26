/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::mock::{mock_key, MockKey};
pub use std::time::{Duration, SystemTimeError};

mock_key! {
    pub struct MockCurrentTime => std::time::SystemTime
}

#[repr(transparent)]
#[derive(Debug, Clone, Copy, PartialOrd, Ord, PartialEq, Eq)]
pub struct SystemTime(pub(super) std::time::SystemTime);

impl From<SystemTime> for ::time::OffsetDateTime {
    fn from(t: SystemTime) -> Self {
        t.0.into()
    }
}

impl SystemTime {
    pub const UNIX_EPOCH: SystemTime = SystemTime(std::time::SystemTime::UNIX_EPOCH);

    pub fn now() -> Self {
        MockCurrentTime.get(|t| SystemTime(*t))
    }

    pub fn duration_since(&self, earlier: Self) -> Result<Duration, SystemTimeError> {
        self.0.duration_since(earlier.0)
    }
}
