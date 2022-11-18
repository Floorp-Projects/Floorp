// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use crate::backend::{
    common::{DatabaseFlags, EnvironmentFlags, WriteFlags},
    traits::{BackendDatabaseFlags, BackendEnvironmentFlags, BackendFlags, BackendWriteFlags},
};

#[derive(Debug, Eq, PartialEq, Copy, Clone, Default)]
pub struct EnvironmentFlagsImpl(pub(crate) lmdb::EnvironmentFlags);

impl BackendFlags for EnvironmentFlagsImpl {
    fn empty() -> EnvironmentFlagsImpl {
        EnvironmentFlagsImpl(lmdb::EnvironmentFlags::empty())
    }
}

impl BackendEnvironmentFlags for EnvironmentFlagsImpl {
    fn set(&mut self, flag: EnvironmentFlags, value: bool) {
        self.0.set(flag.into(), value)
    }
}

impl Into<EnvironmentFlagsImpl> for EnvironmentFlags {
    fn into(self) -> EnvironmentFlagsImpl {
        EnvironmentFlagsImpl(self.into())
    }
}

impl Into<lmdb::EnvironmentFlags> for EnvironmentFlags {
    fn into(self) -> lmdb::EnvironmentFlags {
        match self {
            EnvironmentFlags::FIXED_MAP => lmdb::EnvironmentFlags::FIXED_MAP,
            EnvironmentFlags::NO_SUB_DIR => lmdb::EnvironmentFlags::NO_SUB_DIR,
            EnvironmentFlags::WRITE_MAP => lmdb::EnvironmentFlags::WRITE_MAP,
            EnvironmentFlags::READ_ONLY => lmdb::EnvironmentFlags::READ_ONLY,
            EnvironmentFlags::NO_META_SYNC => lmdb::EnvironmentFlags::NO_META_SYNC,
            EnvironmentFlags::NO_SYNC => lmdb::EnvironmentFlags::NO_SYNC,
            EnvironmentFlags::MAP_ASYNC => lmdb::EnvironmentFlags::MAP_ASYNC,
            EnvironmentFlags::NO_TLS => lmdb::EnvironmentFlags::NO_TLS,
            EnvironmentFlags::NO_LOCK => lmdb::EnvironmentFlags::NO_LOCK,
            EnvironmentFlags::NO_READAHEAD => lmdb::EnvironmentFlags::NO_READAHEAD,
            EnvironmentFlags::NO_MEM_INIT => lmdb::EnvironmentFlags::NO_MEM_INIT,
        }
    }
}

#[derive(Debug, Eq, PartialEq, Copy, Clone, Default)]
pub struct DatabaseFlagsImpl(pub(crate) lmdb::DatabaseFlags);

impl BackendFlags for DatabaseFlagsImpl {
    fn empty() -> DatabaseFlagsImpl {
        DatabaseFlagsImpl(lmdb::DatabaseFlags::empty())
    }
}

impl BackendDatabaseFlags for DatabaseFlagsImpl {
    fn set(&mut self, flag: DatabaseFlags, value: bool) {
        self.0.set(flag.into(), value)
    }
}

impl Into<DatabaseFlagsImpl> for DatabaseFlags {
    fn into(self) -> DatabaseFlagsImpl {
        DatabaseFlagsImpl(self.into())
    }
}

impl Into<lmdb::DatabaseFlags> for DatabaseFlags {
    fn into(self) -> lmdb::DatabaseFlags {
        match self {
            DatabaseFlags::REVERSE_KEY => lmdb::DatabaseFlags::REVERSE_KEY,
            #[cfg(feature = "db-dup-sort")]
            DatabaseFlags::DUP_SORT => lmdb::DatabaseFlags::DUP_SORT,
            #[cfg(feature = "db-dup-sort")]
            DatabaseFlags::DUP_FIXED => lmdb::DatabaseFlags::DUP_FIXED,
            #[cfg(feature = "db-int-key")]
            DatabaseFlags::INTEGER_KEY => lmdb::DatabaseFlags::INTEGER_KEY,
            DatabaseFlags::INTEGER_DUP => lmdb::DatabaseFlags::INTEGER_DUP,
            DatabaseFlags::REVERSE_DUP => lmdb::DatabaseFlags::REVERSE_DUP,
        }
    }
}

#[derive(Debug, Eq, PartialEq, Copy, Clone, Default)]
pub struct WriteFlagsImpl(pub(crate) lmdb::WriteFlags);

impl BackendFlags for WriteFlagsImpl {
    fn empty() -> WriteFlagsImpl {
        WriteFlagsImpl(lmdb::WriteFlags::empty())
    }
}

impl BackendWriteFlags for WriteFlagsImpl {
    fn set(&mut self, flag: WriteFlags, value: bool) {
        self.0.set(flag.into(), value)
    }
}

impl Into<WriteFlagsImpl> for WriteFlags {
    fn into(self) -> WriteFlagsImpl {
        WriteFlagsImpl(self.into())
    }
}

impl Into<lmdb::WriteFlags> for WriteFlags {
    fn into(self) -> lmdb::WriteFlags {
        match self {
            WriteFlags::NO_OVERWRITE => lmdb::WriteFlags::NO_OVERWRITE,
            WriteFlags::NO_DUP_DATA => lmdb::WriteFlags::NO_DUP_DATA,
            WriteFlags::CURRENT => lmdb::WriteFlags::CURRENT,
            WriteFlags::APPEND => lmdb::WriteFlags::APPEND,
            WriteFlags::APPEND_DUP => lmdb::WriteFlags::APPEND_DUP,
        }
    }
}
