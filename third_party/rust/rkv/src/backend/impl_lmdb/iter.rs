// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use super::ErrorImpl;
use crate::backend::traits::BackendIter;

pub struct IterImpl<'i, C> {
    // LMDB semantics dictate that a cursor must be valid for the entire lifetime
    // of an iterator. In other words, cursors must not be dropped while an
    // iterator built from it is alive. Unfortunately, the LMDB crate API does
    // not express this through the type system, so we must enforce it somehow.
    #[allow(dead_code)]
    cursor: C,
    iter: lmdb::Iter<'i>,
}

impl<'i, C> IterImpl<'i, C> {
    pub(crate) fn new(
        mut cursor: C,
        to_iter: impl FnOnce(&mut C) -> lmdb::Iter<'i>,
    ) -> IterImpl<'i, C> {
        let iter = to_iter(&mut cursor);
        IterImpl { cursor, iter }
    }
}

impl<'i, C> BackendIter<'i> for IterImpl<'i, C> {
    type Error = ErrorImpl;

    #[allow(clippy::type_complexity)]
    fn next(&mut self) -> Option<Result<(&'i [u8], &'i [u8]), Self::Error>> {
        self.iter.next().map(|e| e.map_err(ErrorImpl::LmdbError))
    }
}
