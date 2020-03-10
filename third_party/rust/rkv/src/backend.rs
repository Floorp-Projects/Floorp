// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

mod common;
mod impl_lmdb;
mod impl_safe;
mod traits;

pub use common::*;
pub use traits::*;

pub use impl_lmdb::DatabaseImpl as LmdbDatabase;
pub use impl_lmdb::EnvironmentBuilderImpl as Lmdb;
pub use impl_lmdb::EnvironmentImpl as LmdbEnvironment;
pub use impl_lmdb::ErrorImpl as LmdbError;
pub use impl_lmdb::IterImpl as LmdbIter;
pub use impl_lmdb::{
    DatabaseFlagsImpl as LmdbDatabaseFlags,
    EnvironmentFlagsImpl as LmdbEnvironmentFlags,
    WriteFlagsImpl as LmdbWriteFlags,
};
pub use impl_lmdb::{
    InfoImpl as LmdbInfo,
    StatImpl as LmdbStat,
};
pub use impl_lmdb::{
    RoCursorImpl as LmdbRoCursor,
    RwCursorImpl as LmdbRwCursor,
};
pub use impl_lmdb::{
    RoTransactionImpl as LmdbRoTransaction,
    RwTransactionImpl as LmdbRwTransaction,
};

pub use impl_safe::DatabaseImpl as SafeModeDatabase;
pub use impl_safe::EnvironmentBuilderImpl as SafeMode;
pub use impl_safe::EnvironmentImpl as SafeModeEnvironment;
pub use impl_safe::ErrorImpl as SafeModeError;
pub use impl_safe::IterImpl as SafeModeIter;
pub use impl_safe::{
    DatabaseFlagsImpl as SafeModeDatabaseFlags,
    EnvironmentFlagsImpl as SafeModeEnvironmentFlags,
    WriteFlagsImpl as SafeModeWriteFlags,
};
pub use impl_safe::{
    InfoImpl as SafeModeInfo,
    StatImpl as SafeModeStat,
};
pub use impl_safe::{
    RoCursorImpl as SafeModeRoCursor,
    RwCursorImpl as SafeModeRwCursor,
};
pub use impl_safe::{
    RoTransactionImpl as SafeModeRoTransaction,
    RwTransactionImpl as SafeModeRwTransaction,
};
