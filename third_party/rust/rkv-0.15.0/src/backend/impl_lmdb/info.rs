// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use crate::backend::traits::BackendInfo;

pub struct InfoImpl(pub(crate) lmdb::Info);

impl BackendInfo for InfoImpl {
    fn map_size(&self) -> usize {
        self.0.map_size()
    }

    fn last_pgno(&self) -> usize {
        self.0.last_pgno()
    }

    fn last_txnid(&self) -> usize {
        self.0.last_txnid()
    }

    fn max_readers(&self) -> usize {
        self.0.max_readers() as usize
    }

    fn num_readers(&self) -> usize {
        self.0.num_readers() as usize
    }
}
