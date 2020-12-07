// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use crate::backend::traits::BackendStat;

pub struct StatImpl(pub(crate) lmdb::Stat);

impl BackendStat for StatImpl {
    fn page_size(&self) -> usize {
        self.0.page_size() as usize
    }

    fn depth(&self) -> usize {
        self.0.depth() as usize
    }

    fn branch_pages(&self) -> usize {
        self.0.branch_pages()
    }

    fn leaf_pages(&self) -> usize {
        self.0.leaf_pages()
    }

    fn overflow_pages(&self) -> usize {
        self.0.overflow_pages()
    }

    fn entries(&self) -> usize {
        self.0.entries()
    }
}
