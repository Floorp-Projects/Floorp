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
#[cfg(feature = "lmdb")]
mod impl_lmdb;
mod impl_safe;
mod traits;

pub use common::*;
pub use traits::*;

#[cfg(feature = "lmdb")]
pub use impl_lmdb::{
    ArchMigrateError as LmdbArchMigrateError, ArchMigrateResult as LmdbArchMigrateResult,
    ArchMigrator as LmdbArchMigrator, DatabaseFlagsImpl as LmdbDatabaseFlags,
    DatabaseImpl as LmdbDatabase, EnvironmentBuilderImpl as Lmdb,
    EnvironmentFlagsImpl as LmdbEnvironmentFlags, EnvironmentImpl as LmdbEnvironment,
    ErrorImpl as LmdbError, InfoImpl as LmdbInfo, IterImpl as LmdbIter,
    RoCursorImpl as LmdbRoCursor, RoTransactionImpl as LmdbRoTransaction,
    RwCursorImpl as LmdbRwCursor, RwTransactionImpl as LmdbRwTransaction, StatImpl as LmdbStat,
    WriteFlagsImpl as LmdbWriteFlags,
};

pub use impl_safe::{
    DatabaseFlagsImpl as SafeModeDatabaseFlags, DatabaseImpl as SafeModeDatabase,
    EnvironmentBuilderImpl as SafeMode, EnvironmentFlagsImpl as SafeModeEnvironmentFlags,
    EnvironmentImpl as SafeModeEnvironment, ErrorImpl as SafeModeError, InfoImpl as SafeModeInfo,
    IterImpl as SafeModeIter, RoCursorImpl as SafeModeRoCursor,
    RoTransactionImpl as SafeModeRoTransaction, RwCursorImpl as SafeModeRwCursor,
    RwTransactionImpl as SafeModeRwTransaction, StatImpl as SafeModeStat,
    WriteFlagsImpl as SafeModeWriteFlags,
};
