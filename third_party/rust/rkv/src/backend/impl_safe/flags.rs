// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use bitflags::bitflags;
use serde_derive::{Deserialize, Serialize};

use crate::backend::{
    common::{DatabaseFlags, EnvironmentFlags, WriteFlags},
    traits::{BackendDatabaseFlags, BackendEnvironmentFlags, BackendFlags, BackendWriteFlags},
};

bitflags! {
    #[derive(Default, Serialize, Deserialize, PartialEq, Eq, Debug, Clone, Copy)]
    pub struct EnvironmentFlagsImpl: u32 {
        const NIL = 0b0000_0000;
    }
}

impl BackendFlags for EnvironmentFlagsImpl {
    fn empty() -> EnvironmentFlagsImpl {
        EnvironmentFlagsImpl::empty()
    }
}

impl BackendEnvironmentFlags for EnvironmentFlagsImpl {
    fn set(&mut self, flag: EnvironmentFlags, value: bool) {
        self.set(flag.into(), value)
    }
}

impl Into<EnvironmentFlagsImpl> for EnvironmentFlags {
    fn into(self) -> EnvironmentFlagsImpl {
        match self {
            EnvironmentFlags::FIXED_MAP => unimplemented!(),
            EnvironmentFlags::NO_SUB_DIR => unimplemented!(),
            EnvironmentFlags::WRITE_MAP => unimplemented!(),
            EnvironmentFlags::READ_ONLY => unimplemented!(),
            EnvironmentFlags::NO_META_SYNC => unimplemented!(),
            EnvironmentFlags::NO_SYNC => unimplemented!(),
            EnvironmentFlags::MAP_ASYNC => unimplemented!(),
            EnvironmentFlags::NO_TLS => unimplemented!(),
            EnvironmentFlags::NO_LOCK => unimplemented!(),
            EnvironmentFlags::NO_READAHEAD => unimplemented!(),
            EnvironmentFlags::NO_MEM_INIT => unimplemented!(),
        }
    }
}

bitflags! {
    #[derive(Default, Serialize, Deserialize, PartialEq, Eq, Debug, Clone, Copy)]
    pub struct DatabaseFlagsImpl: u32 {
        const NIL = 0b0000_0000;
        #[cfg(feature = "db-dup-sort")]
        const DUP_SORT = 0b0000_0001;
        #[cfg(feature = "db-int-key")]
        const INTEGER_KEY = 0b0000_0010;
    }
}

impl BackendFlags for DatabaseFlagsImpl {
    fn empty() -> DatabaseFlagsImpl {
        DatabaseFlagsImpl::empty()
    }
}

impl BackendDatabaseFlags for DatabaseFlagsImpl {
    fn set(&mut self, flag: DatabaseFlags, value: bool) {
        self.set(flag.into(), value)
    }
}

impl Into<DatabaseFlagsImpl> for DatabaseFlags {
    fn into(self) -> DatabaseFlagsImpl {
        match self {
            DatabaseFlags::REVERSE_KEY => unimplemented!(),
            #[cfg(feature = "db-dup-sort")]
            DatabaseFlags::DUP_SORT => DatabaseFlagsImpl::DUP_SORT,
            #[cfg(feature = "db-dup-sort")]
            DatabaseFlags::DUP_FIXED => unimplemented!(),
            #[cfg(feature = "db-int-key")]
            DatabaseFlags::INTEGER_KEY => DatabaseFlagsImpl::INTEGER_KEY,
            DatabaseFlags::INTEGER_DUP => unimplemented!(),
            DatabaseFlags::REVERSE_DUP => unimplemented!(),
        }
    }
}

bitflags! {
    #[derive(Default, Serialize, Deserialize, PartialEq, Eq, Debug, Clone, Copy)]
    pub struct WriteFlagsImpl: u32 {
        const NIL = 0b0000_0000;
    }
}

impl BackendFlags for WriteFlagsImpl {
    fn empty() -> WriteFlagsImpl {
        WriteFlagsImpl::empty()
    }
}

impl BackendWriteFlags for WriteFlagsImpl {
    fn set(&mut self, flag: WriteFlags, value: bool) {
        self.set(flag.into(), value)
    }
}

impl Into<WriteFlagsImpl> for WriteFlags {
    fn into(self) -> WriteFlagsImpl {
        match self {
            WriteFlags::NO_OVERWRITE => unimplemented!(),
            WriteFlags::NO_DUP_DATA => unimplemented!(),
            WriteFlags::CURRENT => unimplemented!(),
            WriteFlags::APPEND => unimplemented!(),
            WriteFlags::APPEND_DUP => unimplemented!(),
        }
    }
}
