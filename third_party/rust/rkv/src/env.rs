// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{
    fs,
    os::raw::c_uint,
    path::{Path, PathBuf},
};

#[cfg(any(feature = "db-dup-sort", feature = "db-int-key"))]
use crate::backend::{BackendDatabaseFlags, DatabaseFlags};
use crate::{
    backend::{
        BackendEnvironment, BackendEnvironmentBuilder, BackendRoCursorTransaction,
        BackendRwCursorTransaction, SafeModeError,
    },
    error::{CloseError, StoreError},
    readwrite::{Reader, Writer},
    store::{single::SingleStore, CloseOptions, Options as StoreOptions},
};

#[cfg(feature = "db-dup-sort")]
use crate::store::multi::MultiStore;

#[cfg(feature = "db-int-key")]
use crate::store::integer::IntegerStore;
#[cfg(feature = "db-int-key")]
use crate::store::keys::PrimitiveInt;

#[cfg(all(feature = "db-dup-sort", feature = "db-int-key"))]
use crate::store::integermulti::MultiIntegerStore;

pub static DEFAULT_MAX_DBS: c_uint = 5;

/// Wrapper around an `Environment` (e.g. such as an `LMDB` or `SafeMode` environment).
#[derive(Debug)]
pub struct Rkv<E> {
    _path: PathBuf,
    env: E,
}

/// Static methods.
impl<'e, E> Rkv<E>
where
    E: BackendEnvironment<'e>,
{
    pub fn environment_builder<B>() -> B
    where
        B: BackendEnvironmentBuilder<'e, Environment = E>,
    {
        B::new()
    }

    /// Return a new Rkv environment that supports up to `DEFAULT_MAX_DBS` open databases.
    #[allow(clippy::new_ret_no_self)]
    pub fn new<B>(path: &Path) -> Result<Rkv<E>, StoreError>
    where
        B: BackendEnvironmentBuilder<'e, Environment = E>,
    {
        Rkv::with_capacity::<B>(path, DEFAULT_MAX_DBS)
    }

    /// Return a new Rkv environment that supports the specified number of open databases.
    pub fn with_capacity<B>(path: &Path, max_dbs: c_uint) -> Result<Rkv<E>, StoreError>
    where
        B: BackendEnvironmentBuilder<'e, Environment = E>,
    {
        let mut builder = B::new();
        builder.set_max_dbs(max_dbs);

        // Future: set flags, maximum size, etc. here if necessary.
        Rkv::from_builder(path, builder)
    }

    /// Return a new Rkv environment from the provided builder.
    pub fn from_builder<B>(path: &Path, builder: B) -> Result<Rkv<E>, StoreError>
    where
        B: BackendEnvironmentBuilder<'e, Environment = E>,
    {
        Ok(Rkv {
            _path: path.into(),
            env: builder.open(path).map_err(|e| e.into())?,
        })
    }
}

/// Store creation methods.
impl<'e, E> Rkv<E>
where
    E: BackendEnvironment<'e>,
{
    /// Return all created databases.
    pub fn get_dbs(&self) -> Result<Vec<Option<String>>, StoreError> {
        self.env.get_dbs().map_err(|e| e.into())
    }

    /// Create or Open an existing database in (&[u8] -> Single Value) mode.
    /// Note: that create=true cannot be called concurrently with other operations so if
    /// you are sure that the database exists, call this with create=false.
    pub fn open_single<'s, T>(
        &self,
        name: T,
        opts: StoreOptions<E::Flags>,
    ) -> Result<SingleStore<E::Database>, StoreError>
    where
        T: Into<Option<&'s str>>,
    {
        self.open(name, opts).map(SingleStore::new)
    }

    /// Create or Open an existing database in (Integer -> Single Value) mode.
    /// Note: that create=true cannot be called concurrently with other operations so if
    /// you are sure that the database exists, call this with create=false.
    #[cfg(feature = "db-int-key")]
    pub fn open_integer<'s, T, K>(
        &self,
        name: T,
        mut opts: StoreOptions<E::Flags>,
    ) -> Result<IntegerStore<E::Database, K>, StoreError>
    where
        K: PrimitiveInt,
        T: Into<Option<&'s str>>,
    {
        opts.flags.set(DatabaseFlags::INTEGER_KEY, true);
        self.open(name, opts).map(IntegerStore::new)
    }

    /// Create or Open an existing database in (&[u8] -> Multiple Values) mode.
    /// Note: that create=true cannot be called concurrently with other operations so if
    /// you are sure that the database exists, call this with create=false.
    #[cfg(feature = "db-dup-sort")]
    pub fn open_multi<'s, T>(
        &self,
        name: T,
        mut opts: StoreOptions<E::Flags>,
    ) -> Result<MultiStore<E::Database>, StoreError>
    where
        T: Into<Option<&'s str>>,
    {
        opts.flags.set(DatabaseFlags::DUP_SORT, true);
        self.open(name, opts).map(MultiStore::new)
    }

    /// Create or Open an existing database in (Integer -> Multiple Values) mode.
    /// Note: that create=true cannot be called concurrently with other operations so if
    /// you are sure that the database exists, call this with create=false.
    #[cfg(all(feature = "db-dup-sort", feature = "db-int-key"))]
    pub fn open_multi_integer<'s, T, K>(
        &self,
        name: T,
        mut opts: StoreOptions<E::Flags>,
    ) -> Result<MultiIntegerStore<E::Database, K>, StoreError>
    where
        K: PrimitiveInt,
        T: Into<Option<&'s str>>,
    {
        opts.flags.set(DatabaseFlags::INTEGER_KEY, true);
        opts.flags.set(DatabaseFlags::DUP_SORT, true);
        self.open(name, opts).map(MultiIntegerStore::new)
    }

    fn open<'s, T>(&self, name: T, opts: StoreOptions<E::Flags>) -> Result<E::Database, StoreError>
    where
        T: Into<Option<&'s str>>,
    {
        if opts.create {
            self.env
                .create_db(name.into(), opts.flags)
                .map_err(|e| match e.into() {
                    #[cfg(feature = "lmdb")]
                    StoreError::LmdbError(lmdb::Error::BadRslot) => {
                        StoreError::open_during_transaction()
                    }
                    StoreError::SafeModeError(SafeModeError::DbsIllegalOpen) => {
                        StoreError::open_during_transaction()
                    }
                    e => e,
                })
        } else {
            self.env.open_db(name.into()).map_err(|e| match e.into() {
                #[cfg(feature = "lmdb")]
                StoreError::LmdbError(lmdb::Error::BadRslot) => {
                    StoreError::open_during_transaction()
                }
                StoreError::SafeModeError(SafeModeError::DbsIllegalOpen) => {
                    StoreError::open_during_transaction()
                }
                e => e,
            })
        }
    }
}

/// Read and write accessors.
impl<'e, E> Rkv<E>
where
    E: BackendEnvironment<'e>,
{
    /// Create a read transaction.  There can be multiple concurrent readers for an
    /// environment, up to the maximum specified by LMDB (default 126), and you can open
    /// readers while a write transaction is active.
    pub fn read<T>(&'e self) -> Result<Reader<T>, StoreError>
    where
        E: BackendEnvironment<'e, RoTransaction = T>,
        T: BackendRoCursorTransaction<'e, Database = E::Database>,
    {
        Ok(Reader::new(self.env.begin_ro_txn().map_err(|e| e.into())?))
    }

    /// Create a write transaction.  There can be only one write transaction active at any
    /// given time, so trying to create a second one will block until the first is
    /// committed or aborted.
    pub fn write<T>(&'e self) -> Result<Writer<T>, StoreError>
    where
        E: BackendEnvironment<'e, RwTransaction = T>,
        T: BackendRwCursorTransaction<'e, Database = E::Database>,
    {
        Ok(Writer::new(self.env.begin_rw_txn().map_err(|e| e.into())?))
    }
}

/// Other environment methods.
impl<'e, E> Rkv<E>
where
    E: BackendEnvironment<'e>,
{
    /// Flush the data buffers to disk. This call is only useful, when the environment was
    /// open with either `NO_SYNC`, `NO_META_SYNC` or `MAP_ASYNC` (see below). The call is
    /// not valid if the environment was opened with `READ_ONLY`.
    ///
    /// Data is always written to disk when `transaction.commit()` is called, but the
    /// operating system may keep it buffered. LMDB always flushes the OS buffers upon
    /// commit as well, unless the environment was opened with `NO_SYNC` or in part
    /// `NO_META_SYNC`.
    ///
    /// `force`: if true, force a synchronous flush. Otherwise if the environment has the
    /// `NO_SYNC` flag set the flushes will be omitted, and with `MAP_ASYNC` they will
    /// be asynchronous.
    pub fn sync(&self, force: bool) -> Result<(), StoreError> {
        self.env.sync(force).map_err(|e| e.into())
    }

    /// Retrieve statistics about this environment.
    ///
    /// It includes:
    ///   * Page size in bytes
    ///   * B-tree depth
    ///   * Number of internal (non-leaf) pages
    ///   * Number of leaf pages
    ///   * Number of overflow pages
    ///   * Number of data entries
    pub fn stat(&self) -> Result<E::Stat, StoreError> {
        self.env.stat().map_err(|e| e.into())
    }

    /// Retrieve information about this environment.
    ///
    /// It includes:
    ///   * Map size in bytes
    ///   * The last used page number
    ///   * The last transaction ID
    ///   * Max number of readers allowed
    ///   * Number of readers in use
    pub fn info(&self) -> Result<E::Info, StoreError> {
        self.env.info().map_err(|e| e.into())
    }

    /// Retrieve the load ratio (# of used pages / total pages) about this environment.
    ///
    /// With the formular: (last_page_no - freelist_pages) / total_pages.
    /// A value of `None` means that the backend doesn't ever need to be resized.
    pub fn load_ratio(&self) -> Result<Option<f32>, StoreError> {
        self.env.load_ratio().map_err(|e| e.into())
    }

    /// Sets the size of the memory map to use for the environment.
    ///
    /// This can be used to resize the map when the environment is already open. You can
    /// also use `Rkv::environment_builder()` to set the map size during the `Rkv`
    /// initialization.
    ///
    /// Note:
    ///
    /// * No active transactions allowed when performing resizing in this process. It's up
    ///   to the consumer to enforce that.
    ///
    /// * The size should be a multiple of the OS page size. Any attempt to set a size
    ///   smaller than the space already consumed by the environment will be silently
    ///   changed to the current size of the used space.
    ///
    /// * In the multi-process case, once a process resizes the map, other processes need
    ///   to either re-open the environment, or call set_map_size with size 0 to update
    ///   the environment. Otherwise, new transaction creation will fail with
    ///   `LmdbError::MapResized`.
    pub fn set_map_size(&self, size: usize) -> Result<(), StoreError> {
        self.env.set_map_size(size).map_err(Into::into)
    }

    /// Closes this environment and optionally deletes all its files from disk. Doesn't
    /// delete the folder used when opening the environment.
    pub fn close(self, options: CloseOptions) -> Result<(), CloseError> {
        let files = self.env.get_files_on_disk();
        drop(self);

        if options.delete {
            for file in files {
                fs::remove_file(file)?;
            }
        }

        Ok(())
    }
}
