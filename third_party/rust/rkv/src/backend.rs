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
pub use impl_lmdb::RoCursorImpl as LmdbRoCursor;
pub use impl_lmdb::RoTransactionImpl as LmdbRoTransaction;
pub use impl_lmdb::RwTransactionImpl as LmdbRwTransaction;

pub use impl_safe::DatabaseId as SafeModeDatabase;
pub use impl_safe::EnvironmentBuilderImpl as SafeMode;
pub use impl_safe::EnvironmentImpl as SafeModeEnvironment;
pub use impl_safe::ErrorImpl as SafeModeError;
pub use impl_safe::RoCursorImpl as SafeModeRoCursor;
pub use impl_safe::RoTransactionImpl as SafeModeRoTransaction;
pub use impl_safe::RwTransactionImpl as SafeModeRwTransaction;
