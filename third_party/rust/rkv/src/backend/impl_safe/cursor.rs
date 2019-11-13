// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use super::{
    snapshot::Snapshot,
    IterImpl,
};
use crate::backend::traits::BackendRoCursor;

#[derive(Debug)]
pub struct RoCursorImpl<'env>(pub(crate) &'env Snapshot);

impl<'env> BackendRoCursor<'env> for RoCursorImpl<'env> {
    type Iter = IterImpl<'env>;

    fn into_iter(self) -> Self::Iter {
        IterImpl(Box::new(self.0.iter()))
    }

    fn into_iter_from<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]>,
    {
        // FIXME: Don't allocate.
        let key = key.as_ref().to_vec();
        IterImpl(Box::new(self.0.iter().skip_while(move |&(k, _)| k < key.as_slice())))
    }

    fn into_iter_dup_of<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]>,
    {
        // FIXME: Don't allocate.
        let key = key.as_ref().to_vec();
        IterImpl(Box::new(self.0.iter().filter(move |&(k, _)| k == key.as_slice())))
    }
}

#[derive(Debug)]
pub struct RwCursorImpl<'env>(&'env mut Snapshot);

impl<'env> BackendRoCursor<'env> for RwCursorImpl<'env> {
    type Iter = IterImpl<'env>;

    fn into_iter(self) -> Self::Iter {
        unimplemented!()
    }

    fn into_iter_from<K>(self, _key: K) -> Self::Iter
    where
        K: AsRef<[u8]>,
    {
        unimplemented!()
    }

    fn into_iter_dup_of<K>(self, _key: K) -> Self::Iter
    where
        K: AsRef<[u8]>,
    {
        unimplemented!()
    }
}
