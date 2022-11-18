// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

//! A simple utility for migrating data from one RVK environment to another. Notably, this
//! tool can migrate data from an enviroment created with a different backend than the
//! current RKV consumer (e.g from Lmdb to SafeMode).
//!
//! The utility doesn't support migrating between 32-bit and 64-bit LMDB environments yet,
//! see `arch_migrator` if this is needed. However, this utility is ultimately intended to
//! handle all possible migrations.
//!
//! The destination environment should be empty of data, otherwise an error is returned.
//!
//! There are 3 versions of the migration methods:
//! * `migrate_<src>_to_<dst>`, where `<src>` and `<dst>` are the source and destination
//!   environment types. You're responsive with opening both these environments, handling
//!   all errors, and performing any cleanup if necessary.
//! * `open_and_migrate_<src>_to_<dst>`, which is similar the the above, but automatically
//!   attempts to open the source environment and delete all of its supporting files if
//!   there's no other environment open at that path. You're still responsible with
//!   handling all errors.
//! * `easy_migrate_<src>_to_<dst>` which is similar to the above, but ignores the
//!   migration and doesn't delete any files if the source environment is invalid
//!   (corrupted), unavailable (path not accessible or incompatible with configuration),
//!   or empty (database has no records).
//!
//! The tool currently has these limitations:
//!
//! 1. It doesn't support migration from environments created with
//!    `EnvironmentFlags::NO_SUB_DIR`. To migrate such an environment, create a temporary
//!    directory, copy the environment's data files in the temporary directory, then
//!    migrate the temporary directory as the source environment.
//! 2. It doesn't support migration from databases created with DatabaseFlags::DUP_SORT`
//!    (with or without `DatabaseFlags::DUP_FIXED`) nor with `DatabaseFlags::INTEGER_KEY`.
//!    This effectively means that migration is limited to `SingleStore`s.
//! 3. It doesn't allow for existing data in the destination environment, which means that
//!    it cannot overwrite nor append data.

use crate::{
    backend::{LmdbEnvironment, SafeModeEnvironment},
    error::MigrateError,
    Rkv, StoreOptions,
};

pub use crate::backend::{LmdbArchMigrateError, LmdbArchMigrateResult, LmdbArchMigrator};

// FIXME: should parametrize this instead.

macro_rules! fn_migrator {
    ($name:tt, $src_env:ty, $dst_env:ty) => {
        /// Migrate all data in all of databases from the source environment to the destination
        /// environment. This includes all key/value pairs in the main database that aren't
        /// metadata about subdatabases and all key/value pairs in all subdatabases.
        ///
        /// Other backend-specific metadata such as map size or maximum databases left intact on
        /// the given environments.
        ///
        /// The destination environment should be empty of data, otherwise an error is returned.
        pub fn $name<S, D>(src_env: S, dst_env: D) -> Result<(), MigrateError>
        where
            S: std::ops::Deref<Target = Rkv<$src_env>>,
            D: std::ops::Deref<Target = Rkv<$dst_env>>,
        {
            let src_dbs = src_env.get_dbs().unwrap();
            if src_dbs.is_empty() {
                return Err(MigrateError::SourceEmpty);
            }
            let dst_dbs = dst_env.get_dbs().unwrap();
            if !dst_dbs.is_empty() {
                return Err(MigrateError::DestinationNotEmpty);
            }
            for name in src_dbs {
                let src_store = src_env.open_single(name.as_deref(), StoreOptions::default())?;
                let dst_store = dst_env.open_single(name.as_deref(), StoreOptions::create())?;
                let reader = src_env.read()?;
                let mut writer = dst_env.write()?;
                let mut iter = src_store.iter_start(&reader)?;
                while let Some(Ok((key, value))) = iter.next() {
                    dst_store.put(&mut writer, key, &value).expect("wrote");
                }
                writer.commit()?;
            }
            Ok(())
        }
    };

    (open $migrate:tt, $name:tt, $builder:tt, $src_env:ty, $dst_env:ty) => {
        /// Same as the the `migrate_x_to_y` migration method above, but automatically attempts
        /// to open the source environment. Finally, deletes all of its supporting files if
        /// there's no other environment open at that path and the migration succeeded.
        pub fn $name<F, D>(path: &std::path::Path, build: F, dst_env: D) -> Result<(), MigrateError>
        where
            F: FnOnce(crate::backend::$builder) -> crate::backend::$builder,
            D: std::ops::Deref<Target = Rkv<$dst_env>>,
        {
            use crate::{backend::*, CloseOptions};

            let mut manager = crate::Manager::<$src_env>::singleton().write()?;
            let mut builder = Rkv::<$src_env>::environment_builder::<$builder>();
            builder.set_max_dbs(crate::env::DEFAULT_MAX_DBS);
            builder = build(builder);

            let src_env =
                manager.get_or_create_from_builder(path, builder, Rkv::from_builder::<$builder>)?;
            Migrator::$migrate(src_env.read()?, dst_env)?;

            drop(src_env);
            manager.try_close(path, CloseOptions::delete_files_on_disk())?;

            Ok(())
        }
    };

    (easy $migrate:tt, $name:tt, $src_env:ty, $dst_env:ty) => {
        /// Same as the `open_and_migrate_x_to_y` migration method above, but ignores the
        /// migration and doesn't delete any files if the following conditions apply:
        /// - Source environment is invalid/corrupted, unavailable, or empty.
        /// - Destination environment is not empty.
        /// Use this instead of the other migration methods if:
        /// - You're not concerned by throwing away old data and starting fresh with a new store.
        /// - You'll never want to overwrite data in the new store from the old store.
        pub fn $name<D>(path: &std::path::Path, dst_env: D) -> Result<(), MigrateError>
        where
            D: std::ops::Deref<Target = Rkv<$dst_env>>,
        {
            match Migrator::$migrate(path, |builder| builder, dst_env) {
                // Source environment is an invalid file or corrupted database.
                Err(crate::MigrateError::StoreError(crate::StoreError::FileInvalid)) => Ok(()),
                Err(crate::MigrateError::StoreError(crate::StoreError::DatabaseCorrupted)) => {
                    Ok(())
                }
                // Path not accessible.
                Err(crate::MigrateError::StoreError(crate::StoreError::IoError(_))) => Ok(()),
                // Path accessible but incompatible for configuration.
                Err(crate::MigrateError::StoreError(
                    crate::StoreError::UnsuitableEnvironmentPath(_),
                )) => Ok(()),
                // Couldn't close source environment and delete files on disk (e.g. other stores still open).
                Err(crate::MigrateError::CloseError(_)) => Ok(()),
                // Nothing to migrate.
                Err(crate::MigrateError::SourceEmpty) => Ok(()),
                // Migrating would overwrite.
                Err(crate::MigrateError::DestinationNotEmpty) => Ok(()),
                result => result,
            }?;

            Ok(())
        }
    };
}

macro_rules! fns_migrator {
    ($src:tt, $dst:tt) => {
        paste::item! {
            fns_migrator!([<migrate_ $src _to_ $dst>], $src, $dst);
            fns_migrator!([<migrate_ $dst _to_ $src>], $dst, $src);
        }
    };
    ($name:tt, $src:tt, $dst:tt) => {
        paste::item! {
            fn_migrator!($name, [<$src:camel Environment>], [<$dst:camel Environment>]);
            fn_migrator!(open $name, [<open_and_ $name>], [<$src:camel>], [<$src:camel Environment>], [<$dst:camel Environment>]);
            fn_migrator!(easy [<open_and_ $name>], [<easy_ $name>], [<$src:camel Environment>], [<$dst:camel Environment>]);
        }
    };
}

pub struct Migrator;

impl Migrator {
    fns_migrator!(lmdb, safe_mode);
}
