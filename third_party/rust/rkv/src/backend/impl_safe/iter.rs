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

// FIXME: Use generics instead.
pub struct IterImpl<'env>(pub(crate) Box<dyn Iterator<Item = (&'env [u8], &'env [u8])> + 'env>);

impl<'env> BackendIter<'env> for IterImpl<'env> {
    type Error = ErrorImpl;

    #[allow(clippy::type_complexity)]
    fn next(&mut self) -> Option<Result<(&'env [u8], &'env [u8]), Self::Error>> {
        self.0.next().map(Ok)
    }
}
