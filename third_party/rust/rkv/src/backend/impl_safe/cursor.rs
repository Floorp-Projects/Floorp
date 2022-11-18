// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use super::{snapshot::Snapshot, IterImpl};
use crate::backend::traits::BackendRoCursor;

#[derive(Debug)]
pub struct RoCursorImpl<'c>(pub(crate) &'c Snapshot);

#[cfg(not(feature = "db-dup-sort"))]
impl<'c> BackendRoCursor<'c> for RoCursorImpl<'c> {
    type Iter = IterImpl<'c>;

    fn into_iter(self) -> Self::Iter {
        IterImpl(Box::new(self.0.iter()))
    }

    fn into_iter_from<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        IterImpl(Box::new(
            self.0.iter().skip_while(move |&(k, _)| k < key.as_ref()),
        ))
    }

    fn into_iter_dup_of<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        IterImpl(Box::new(
            self.0.iter().filter(move |&(k, _)| k == key.as_ref()),
        ))
    }
}

#[cfg(feature = "db-dup-sort")]
impl<'c> BackendRoCursor<'c> for RoCursorImpl<'c> {
    type Iter = IterImpl<'c>;

    fn into_iter(self) -> Self::Iter {
        let flattened = self
            .0
            .iter()
            .flat_map(|(key, values)| values.map(move |value| (key, value)));
        IterImpl(Box::new(flattened))
    }

    fn into_iter_from<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        let skipped = self.0.iter().skip_while(move |&(k, _)| k < key.as_ref());
        let flattened = skipped.flat_map(|(key, values)| values.map(move |value| (key, value)));
        IterImpl(Box::new(flattened))
    }

    fn into_iter_dup_of<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        let filtered = self.0.iter().filter(move |&(k, _)| k == key.as_ref());
        let flattened = filtered.flat_map(|(key, values)| values.map(move |value| (key, value)));
        IterImpl(Box::new(flattened))
    }
}

#[derive(Debug)]
pub struct RwCursorImpl<'c>(&'c mut Snapshot);

impl<'c> BackendRoCursor<'c> for RwCursorImpl<'c> {
    type Iter = IterImpl<'c>;

    fn into_iter(self) -> Self::Iter {
        unimplemented!()
    }

    fn into_iter_from<K>(self, _key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        unimplemented!()
    }

    fn into_iter_dup_of<K>(self, _key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c,
    {
        unimplemented!()
    }
}
