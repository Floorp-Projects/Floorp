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
use serde_derive::{Deserialize, Serialize};

use super::{snapshot::Snapshot, DatabaseFlagsImpl};
use crate::backend::traits::BackendDatabase;

#[derive(Debug, Eq, PartialEq, Copy, Clone, Hash)]
pub struct DatabaseImpl(pub(crate) Id<Database>);

impl BackendDatabase for DatabaseImpl {}

#[derive(Debug, Serialize, Deserialize)]
pub struct Database {
    snapshot: Snapshot,
}

impl Database {
    pub(crate) fn new(flags: Option<DatabaseFlagsImpl>, snapshot: Option<Snapshot>) -> Database {
        Database {
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
