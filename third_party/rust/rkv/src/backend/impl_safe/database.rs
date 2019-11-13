// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use id_arena::Id;
use serde_derive::{
    Deserialize,
    Serialize,
};

use super::snapshot::Snapshot;
use super::DatabaseFlagsImpl;
use crate::backend::traits::BackendDatabase;

pub type DatabaseId = Id<DatabaseImpl>;

impl BackendDatabase for DatabaseId {}

#[derive(Debug, Serialize, Deserialize)]
pub struct DatabaseImpl {
    snapshot: Snapshot,
}

impl DatabaseImpl {
    pub(crate) fn new(flags: Option<DatabaseFlagsImpl>, snapshot: Option<Snapshot>) -> DatabaseImpl {
        DatabaseImpl {
            snapshot: snapshot.unwrap_or_else(|| Snapshot::new(flags)),
        }
    }

    pub(crate) fn snapshot(&self) -> Snapshot {
        self.snapshot.clone()
    }

    pub(crate) fn replace(&mut self, snapshot: Snapshot) -> Snapshot {
        std::mem::replace(&mut self.snapshot, snapshot)
    }
}
