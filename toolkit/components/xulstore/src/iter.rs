/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::{XULStoreError, XULStoreResult};
use std::vec::IntoIter;

pub struct XULStoreIterator {
    values: IntoIter<String>,
}

impl XULStoreIterator {
    pub(crate) fn new(values: IntoIter<String>) -> Self {
        Self { values }
    }

    pub(crate) fn has_more(&self) -> bool {
        !self.values.as_slice().is_empty()
    }

    pub(crate) fn get_next(&mut self) -> XULStoreResult<String> {
        Ok(self.values.next().ok_or(XULStoreError::IterationFinished)?)
    }
}
