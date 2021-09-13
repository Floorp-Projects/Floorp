// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::btree_map::Entry;
use std::collections::BTreeMap;
use std::fs;
use std::num::NonZeroU64;
use std::path::Path;
use std::str;
use std::sync::RwLock;

use rkv::StoreOptions;

// Select the LMDB-powered storage backend when the feature is not activated.
#[cfg(not(feature = "rkv-safe-mode"))]
mod backend {
    use std::path::Path;

    /// cbindgen:ignore
    pub type Rkv = rkv::Rkv<rkv::backend::LmdbEnvironment>;
    /// cbindgen:ignore
    pub type SingleStore = rkv::SingleStore<rkv::backend::LmdbDatabase>;
    /// cbindgen:ignore
    pub type Writer<'t> = rkv::Writer<rkv::backend::LmdbRwTransaction<'t>>;

    pub fn rkv_new(path: &Path) -> Result<Rkv, rkv::StoreError> {
        Rkv::new::<rkv::backend::Lmdb>(path)
    }

    /// No migration necessary when staying with LMDB.
    pub fn migrate(_path: &Path, _dst_env: &Rkv) {
        // Intentionally left empty.
    }
}

// Select the "safe mode" storage backend when the feature is activated.
#[cfg(feature = "rkv-safe-mode")]
mod backend {
    use rkv::migrator::Migrator;
    use std::{fs, path::Path};

    /// cbindgen:ignore
    pub type Rkv = rkv::Rkv<rkv::backend::SafeModeEnvironment>;
    /// cbindgen:ignore
    pub type SingleStore = rkv::SingleStore<rkv::backend::SafeModeDatabase>;
    /// cbindgen:ignore
    pub type Writer<'t> = rkv::Writer<rkv::backend::SafeModeRwTransaction<'t>>;

    pub fn rkv_new(path: &Path) -> Result<Rkv, rkv::StoreError> {
        match Rkv::new::<rkv::backend::SafeMode>(path) {
            // An invalid file can mean:
            // 1. An empty file.
            // 2. A corrupted file.
            //
            // In both instances there's not much we can do.
            // Drop the data by removing the file, and start over.
            Err(rkv::StoreError::FileInvalid) => {
                let safebin = path.join("data.safe.bin");
                fs::remove_file(safebin).map_err(|_| rkv::StoreError::FileInvalid)?;
                // Now try again, we only handle that error once.
                Rkv::new::<rkv::backend::SafeMode>(path)
            }
            other => other,
        }
    }

    fn delete_and_log(path: &Path, msg: &str) {
        if let Err(err) = fs::remove_file(path) {
            match err.kind() {
                std::io::ErrorKind::NotFound => {
                    // Silently drop this error, the file was already non-existing.
                }
                _ => log::warn!("{}", msg),
            }
        }
    }

    fn delete_lmdb_database(path: &Path) {
        let datamdb = path.join("data.mdb");
        delete_and_log(&datamdb, "Failed to delete old data.");

        let lockmdb = path.join("lock.mdb");
        delete_and_log(&lockmdb, "Failed to delete old lock.");
    }

    /// Migrate from LMDB storage to safe-mode storage.
    ///
    /// This migrates the data once, then deletes the LMDB storage.
    /// The safe-mode storage must be empty for it to work.
    /// Existing data will not be overwritten.
    /// If the destination database is not empty the LMDB database is deleted
    /// without migrating data.
    /// This is a no-op if no LMDB database file exists.
    pub fn migrate(path: &Path, dst_env: &Rkv) {
        use rkv::{MigrateError, StoreError};

        log::debug!("Migrating files in {}", path.display());

        // Shortcut if no data to migrate is around.
        let datamdb = path.join("data.mdb");
        if !datamdb.exists() {
            log::debug!("No data to migrate.");
            return;
        }

        // We're handling the same error cases as `easy_migrate_lmdb_to_safe_mode`,
        // but annotate each why they don't cause problems for Glean.
        // Additionally for known cases we delete the LMDB database regardless.
        let should_delete =
            match Migrator::open_and_migrate_lmdb_to_safe_mode(path, |builder| builder, dst_env) {
                // Source environment is corrupted.
                // We start fresh with the new database.
                Err(MigrateError::StoreError(StoreError::FileInvalid)) => true,
                Err(MigrateError::StoreError(StoreError::DatabaseCorrupted)) => true,
                // Path not accessible.
                // Somehow our directory vanished between us creating it and reading from it.
                // Nothing we can do really.
                Err(MigrateError::StoreError(StoreError::IoError(_))) => true,
                // Path accessible but incompatible for configuration.
                // This should not happen, we never used storages that safe-mode doesn't understand.
                // If it does happen, let's start fresh and use the safe-mode from now on.
                Err(MigrateError::StoreError(StoreError::UnsuitableEnvironmentPath(_))) => true,
                // Nothing to migrate.
                // Source database was empty. We just start fresh anyway.
                Err(MigrateError::SourceEmpty) => true,
                // Migrating would overwrite.
                // Either a previous migration failed and we still started writing data,
                // or someone placed back an old data file.
                // In any case we better stay on the new data and delete the old one.
                Err(MigrateError::DestinationNotEmpty) => {
                    log::warn!("Failed to migrate old data. Destination was not empty");
                    true
                }
                // An internal lock was poisoned.
                // This would only happen if multiple things run concurrently and one crashes.
                Err(MigrateError::ManagerPoisonError) => false,
                // Couldn't close source environment and delete files on disk (e.g. other stores still open).
                // This could only happen if multiple instances are running,
                // we leave files in place.
                Err(MigrateError::CloseError(_)) => false,
                // Other store errors are never returned from the migrator.
                // We need to handle them to please rustc.
                Err(MigrateError::StoreError(_)) => false,
                // Other errors can't happen, so this leaves us with the Ok case.
                // This already deleted the LMDB files.
                Ok(()) => false,
            };

        if should_delete {
            log::debug!("Need to delete remaining LMDB files.");
            delete_lmdb_database(path);
        }

        log::debug!("Migration ended. Safe-mode database in {}", path.display());
    }
}

use crate::metrics::Metric;
use crate::CommonMetricData;
use crate::Glean;
use crate::Lifetime;
use crate::Result;
use backend::*;

pub struct Database {
    /// Handle to the database environment.
    rkv: Rkv,

    /// Handles to the "lifetime" stores.
    ///
    /// A "store" is a handle to the underlying database.
    /// We keep them open for fast and frequent access.
    user_store: SingleStore,
    ping_store: SingleStore,
    application_store: SingleStore,

    /// If the `delay_ping_lifetime_io` Glean config option is `true`,
    /// we will save metrics with 'ping' lifetime data in a map temporarily
    /// so as to persist them to disk using rkv in bulk on demand.
    ping_lifetime_data: Option<RwLock<BTreeMap<String, Metric>>>,

    // Initial file size when opening the database.
    file_size: Option<NonZeroU64>,
}

impl std::fmt::Debug for Database {
    fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
        fmt.debug_struct("Database")
            .field("rkv", &self.rkv)
            .field("user_store", &"SingleStore")
            .field("ping_store", &"SingleStore")
            .field("application_store", &"SingleStore")
            .field("ping_lifetime_data", &self.ping_lifetime_data)
            .finish()
    }
}

/// Calculate the  database size from all the files in the directory.
///
///  # Arguments
///
///  *`path` - The path to the directory
///
///  # Returns
///
/// Returns the non-zero combined size of all files in a directory,
/// or `None` on error or if the size is `0`.
fn database_size(dir: &Path) -> Option<NonZeroU64> {
    let mut total_size = 0;
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            if let Ok(file_type) = entry.file_type() {
                if file_type.is_file() {
                    let path = entry.path();
                    if let Ok(metadata) = fs::metadata(path) {
                        total_size += metadata.len();
                    } else {
                        continue;
                    }
                }
            }
        }
    }

    NonZeroU64::new(total_size)
}

impl Database {
    /// Initializes the data store.
    ///
    /// This opens the underlying rkv store and creates
    /// the underlying directory structure.
    ///
    /// It also loads any Lifetime::Ping data that might be
    /// persisted, in case `delay_ping_lifetime_io` is set.
    pub fn new(data_path: &Path, delay_ping_lifetime_io: bool) -> Result<Self> {
        #[cfg(all(windows, not(feature = "rkv-safe-mode")))]
        {
            // The underlying lmdb wrapper implementation
            // cannot actually handle non-UTF8 paths on Windows.
            // It will unconditionally panic if passed one.
            // See
            // https://github.com/mozilla/lmdb-rs/blob/df1c2f56e3088f097c719c57b9925ab51e26f3f4/src/environment.rs#L43-L53
            //
            // To avoid this, in case we're using LMDB on Windows (that's just testing now though),
            // we simply error out earlier.
            if data_path.to_str().is_none() {
                return Err(crate::Error::utf8_error());
            }
        }

        let path = data_path.join("db");
        log::debug!("Database path: {:?}", path.display());
        let file_size = database_size(&path);

        let rkv = Self::open_rkv(&path)?;
        let user_store = rkv.open_single(Lifetime::User.as_str(), StoreOptions::create())?;
        let ping_store = rkv.open_single(Lifetime::Ping.as_str(), StoreOptions::create())?;
        let application_store =
            rkv.open_single(Lifetime::Application.as_str(), StoreOptions::create())?;
        let ping_lifetime_data = if delay_ping_lifetime_io {
            Some(RwLock::new(BTreeMap::new()))
        } else {
            None
        };

        let db = Self {
            rkv,
            user_store,
            ping_store,
            application_store,
            ping_lifetime_data,
            file_size,
        };

        db.load_ping_lifetime_data();

        Ok(db)
    }

    /// Get the initial database file size.
    pub fn file_size(&self) -> Option<NonZeroU64> {
        self.file_size
    }

    fn get_store(&self, lifetime: Lifetime) -> &SingleStore {
        match lifetime {
            Lifetime::User => &self.user_store,
            Lifetime::Ping => &self.ping_store,
            Lifetime::Application => &self.application_store,
        }
    }

    /// Creates the storage directories and inits rkv.
    fn open_rkv(path: &Path) -> Result<Rkv> {
        fs::create_dir_all(&path)?;

        let rkv = rkv_new(path)?;
        migrate(path, &rkv);

        log::info!("Database initialized");
        Ok(rkv)
    }

    /// Build the key of the final location of the data in the database.
    /// Such location is built using the storage name and the metric
    /// key/name (if available).
    ///
    /// # Arguments
    ///
    /// * `storage_name` - the name of the storage to store/fetch data from.
    /// * `metric_key` - the optional metric key/name.
    ///
    /// # Returns
    ///
    /// A string representing the location in the database.
    fn get_storage_key(storage_name: &str, metric_key: Option<&str>) -> String {
        match metric_key {
            Some(k) => format!("{}#{}", storage_name, k),
            None => format!("{}#", storage_name),
        }
    }

    /// Loads Lifetime::Ping data from rkv to memory,
    /// if `delay_ping_lifetime_io` is set to true.
    ///
    /// Does nothing if it isn't or if there is not data to load.
    fn load_ping_lifetime_data(&self) {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            let mut data = ping_lifetime_data
                .write()
                .expect("Can't read ping lifetime data");

            let reader = unwrap_or!(self.rkv.read(), return);
            let store = self.get_store(Lifetime::Ping);
            let mut iter = unwrap_or!(store.iter_start(&reader), return);

            while let Some(Ok((metric_id, value))) = iter.next() {
                let metric_id = match str::from_utf8(metric_id) {
                    Ok(metric_id) => metric_id.to_string(),
                    _ => continue,
                };
                let metric: Metric = match value {
                    rkv::Value::Blob(blob) => unwrap_or!(bincode::deserialize(blob), continue),
                    _ => continue,
                };

                data.insert(metric_id, metric);
            }
        }
    }

    /// Iterates with the provided transaction function
    /// over the requested data from the given storage.
    ///
    /// * If the storage is unavailable, the transaction function is never invoked.
    /// * If the read data cannot be deserialized it will be silently skipped.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - The metric lifetime to iterate over.
    /// * `storage_name` - The storage name to iterate over.
    /// * `metric_key` - The metric key to iterate over. All metrics iterated over
    ///   will have this prefix. For example, if `metric_key` is of the form `{category}.`,
    ///   it will iterate over all metrics in the given category. If the `metric_key` is of the
    ///   form `{category}.{name}/`, the iterator will iterate over all specific metrics for
    ///   a given labeled metric. If not provided, the entire storage for the given lifetime
    ///   will be iterated over.
    /// * `transaction_fn` - Called for each entry being iterated over. It is
    ///   passed two arguments: `(metric_id: &[u8], metric: &Metric)`.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn iter_store_from<F>(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_key: Option<&str>,
        mut transaction_fn: F,
    ) where
        F: FnMut(&[u8], &Metric),
    {
        let iter_start = Self::get_storage_key(storage_name, metric_key);
        let len = iter_start.len();

        // Lifetime::Ping data is not immediately persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let data = ping_lifetime_data
                    .read()
                    .expect("Can't read ping lifetime data");
                for (key, value) in data.iter() {
                    if key.starts_with(&iter_start) {
                        let key = &key[len..];
                        transaction_fn(key.as_bytes(), value);
                    }
                }
                return;
            }
        }

        let reader = unwrap_or!(self.rkv.read(), return);
        let mut iter = unwrap_or!(
            self.get_store(lifetime).iter_from(&reader, &iter_start),
            return
        );

        while let Some(Ok((metric_id, value))) = iter.next() {
            if !metric_id.starts_with(iter_start.as_bytes()) {
                break;
            }

            let metric_id = &metric_id[len..];
            let metric: Metric = match value {
                rkv::Value::Blob(blob) => unwrap_or!(bincode::deserialize(blob), continue),
                _ => continue,
            };
            transaction_fn(metric_id, &metric);
        }
    }

    /// Determines if the storage has the given metric.
    ///
    /// If data cannot be read it is assumed that the storage does not have the metric.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - The lifetime of the metric.
    /// * `storage_name` - The storage name to look in.
    /// * `metric_identifier` - The metric identifier.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn has_metric(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_identifier: &str,
    ) -> bool {
        let key = Self::get_storage_key(storage_name, Some(metric_identifier));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                return ping_lifetime_data
                    .read()
                    .map(|data| data.contains_key(&key))
                    .unwrap_or(false);
            }
        }

        let reader = unwrap_or!(self.rkv.read(), return false);
        self.get_store(lifetime)
            .get(&reader, &key)
            .unwrap_or(None)
            .is_some()
    }

    /// Writes to the specified storage with the provided transaction function.
    ///
    /// If the storage is unavailable, it will return an error.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    fn write_with_store<F>(&self, store_name: Lifetime, mut transaction_fn: F) -> Result<()>
    where
        F: FnMut(Writer, &SingleStore) -> Result<()>,
    {
        let writer = self.rkv.write().unwrap();
        let store = self.get_store(store_name);
        transaction_fn(writer, store)
    }

    /// Records a metric in the underlying storage system.
    pub fn record(&self, glean: &Glean, data: &CommonMetricData, value: &Metric) {
        // If upload is disabled we don't want to record.
        if !glean.is_upload_enabled() {
            return;
        }

        let name = data.identifier(glean);

        for ping_name in data.storage_names() {
            if let Err(e) = self.record_per_lifetime(data.lifetime, ping_name, &name, value) {
                log::error!("Failed to record metric into {}: {:?}", ping_name, e);
            }
        }
    }

    /// Records a metric in the underlying storage system, for a single lifetime.
    ///
    /// # Returns
    ///
    /// If the storage is unavailable or the write fails, no data will be stored and an error will be returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    fn record_per_lifetime(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        key: &str,
        metric: &Metric,
    ) -> Result<()> {
        let final_key = Self::get_storage_key(storage_name, Some(key));

        // Lifetime::Ping data is not immediately persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't read ping lifetime data");
                data.insert(final_key, metric.clone());
                return Ok(());
            }
        }

        let encoded = bincode::serialize(&metric).expect("IMPOSSIBLE: Serializing metric failed");
        let value = rkv::Value::Blob(&encoded);

        let mut writer = self.rkv.write()?;
        self.get_store(lifetime)
            .put(&mut writer, final_key, &value)?;
        writer.commit()?;
        Ok(())
    }

    /// Records the provided value, with the given lifetime,
    /// after applying a transformation function.
    pub fn record_with<F>(&self, glean: &Glean, data: &CommonMetricData, mut transform: F)
    where
        F: FnMut(Option<Metric>) -> Metric,
    {
        // If upload is disabled we don't want to record.
        if !glean.is_upload_enabled() {
            return;
        }

        let name = data.identifier(glean);
        for ping_name in data.storage_names() {
            if let Err(e) =
                self.record_per_lifetime_with(data.lifetime, ping_name, &name, &mut transform)
            {
                log::error!("Failed to record metric into {}: {:?}", ping_name, e);
            }
        }
    }

    /// Records a metric in the underlying storage system,
    /// after applying the given transformation function, for a single lifetime.
    ///
    /// # Returns
    ///
    /// If the storage is unavailable or the write fails, no data will be stored and an error will be returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    fn record_per_lifetime_with<F>(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        key: &str,
        mut transform: F,
    ) -> Result<()>
    where
        F: FnMut(Option<Metric>) -> Metric,
    {
        let final_key = Self::get_storage_key(storage_name, Some(key));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't access ping lifetime data as writable");
                let entry = data.entry(final_key);
                match entry {
                    Entry::Vacant(entry) => {
                        entry.insert(transform(None));
                    }
                    Entry::Occupied(mut entry) => {
                        let old_value = entry.get().clone();
                        entry.insert(transform(Some(old_value)));
                    }
                }
                return Ok(());
            }
        }

        let mut writer = self.rkv.write()?;
        let store = self.get_store(lifetime);
        let new_value: Metric = {
            let old_value = store.get(&writer, &final_key)?;

            match old_value {
                Some(rkv::Value::Blob(blob)) => {
                    let old_value = bincode::deserialize(blob).ok();
                    transform(old_value)
                }
                _ => transform(None),
            }
        };

        let encoded =
            bincode::serialize(&new_value).expect("IMPOSSIBLE: Serializing metric failed");
        let value = rkv::Value::Blob(&encoded);
        store.put(&mut writer, final_key, &value)?;
        writer.commit()?;
        Ok(())
    }

    /// Clears a storage (only Ping Lifetime).
    ///
    /// # Returns
    ///
    /// * If the storage is unavailable an error is returned.
    /// * If any individual delete fails, an error is returned, but other deletions might have
    ///   happened.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn clear_ping_lifetime_storage(&self, storage_name: &str) -> Result<()> {
        // Lifetime::Ping data will be saved to `ping_lifetime_data`
        // in case `delay_ping_lifetime_io` is set to true
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            ping_lifetime_data
                .write()
                .expect("Can't access ping lifetime data as writable")
                .clear();
        }

        self.write_with_store(Lifetime::Ping, |mut writer, store| {
            let mut metrics = Vec::new();
            {
                let mut iter = store.iter_from(&writer, &storage_name)?;
                while let Some(Ok((metric_id, _))) = iter.next() {
                    if let Ok(metric_id) = std::str::from_utf8(metric_id) {
                        if !metric_id.starts_with(&storage_name) {
                            break;
                        }
                        metrics.push(metric_id.to_owned());
                    }
                }
            }

            let mut res = Ok(());
            for to_delete in metrics {
                if let Err(e) = store.delete(&mut writer, to_delete) {
                    log::warn!("Can't delete from store: {:?}", e);
                    res = Err(e);
                }
            }

            writer.commit()?;
            Ok(res?)
        })
    }

    /// Removes a single metric from the storage.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - the lifetime of the storage in which to look for the metric.
    /// * `storage_name` - the name of the storage to store/fetch data from.
    /// * `metric_id` - the metric category + name.
    ///
    /// # Returns
    ///
    /// * If the storage is unavailable an error is returned.
    /// * If the metric could not be deleted, an error is returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn remove_single_metric(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_id: &str,
    ) -> Result<()> {
        let final_key = Self::get_storage_key(storage_name, Some(metric_id));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't access app lifetime data as writable");
                data.remove(&final_key);
            }
        }

        self.write_with_store(lifetime, |mut writer, store| {
            if let Err(e) = store.delete(&mut writer, final_key.clone()) {
                if self.ping_lifetime_data.is_some() {
                    // If ping_lifetime_data exists, it might be
                    // that data is in memory, but not yet in rkv.
                    return Ok(());
                }
                return Err(e.into());
            }
            writer.commit()?;
            Ok(())
        })
    }

    /// Clears all the metrics in the database, for the provided lifetime.
    ///
    /// Errors are logged.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn clear_lifetime(&self, lifetime: Lifetime) {
        let res = self.write_with_store(lifetime, |mut writer, store| {
            store.clear(&mut writer)?;
            writer.commit()?;
            Ok(())
        });
        if let Err(e) = res {
            log::warn!("Could not clear store for lifetime {:?}: {:?}", lifetime, e);
        }
    }

    /// Clears all metrics in the database.
    ///
    /// Errors are logged.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn clear_all(&self) {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            ping_lifetime_data
                .write()
                .expect("Can't access ping lifetime data as writable")
                .clear();
        }

        for lifetime in [Lifetime::User, Lifetime::Ping, Lifetime::Application].iter() {
            self.clear_lifetime(*lifetime);
        }
    }

    /// Persists ping_lifetime_data to disk.
    ///
    /// Does nothing in case there is nothing to persist.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn persist_ping_lifetime_data(&self) -> Result<()> {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            let data = ping_lifetime_data
                .read()
                .expect("Can't read ping lifetime data");

            self.write_with_store(Lifetime::Ping, |mut writer, store| {
                for (key, value) in data.iter() {
                    let encoded =
                        bincode::serialize(&value).expect("IMPOSSIBLE: Serializing metric failed");
                    // There is no need for `get_storage_key` here because
                    // the key is already formatted from when it was saved
                    // to ping_lifetime_data.
                    store.put(&mut writer, &key, &rkv::Value::Blob(&encoded))?;
                }
                writer.commit()?;
                Ok(())
            })?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;
    use crate::CommonMetricData;
    use std::collections::HashMap;
    use std::path::Path;
    use tempfile::tempdir;

    #[test]
    fn test_panicks_if_fails_dir_creation() {
        let path = Path::new("/!#\"'@#°ç");
        assert!(Database::new(path, false).is_err());
    }

    #[test]
    #[cfg(windows)]
    fn windows_invalid_utf16_panicfree() {
        use std::ffi::OsString;
        use std::os::windows::prelude::*;

        // Here the values 0x0066 and 0x006f correspond to 'f' and 'o'
        // respectively. The value 0xD800 is a lone surrogate half, invalid
        // in a UTF-16 sequence.
        let source = [0x0066, 0x006f, 0xD800, 0x006f];
        let os_string = OsString::from_wide(&source[..]);
        let os_str = os_string.as_os_str();
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false);

        #[cfg(feature = "rkv-safe-mode")]
        {
            assert!(
                res.is_ok(),
                "Database should succeed at {}: {:?}",
                path.display(),
                res
            );
        }

        #[cfg(not(feature = "rkv-safe-mode"))]
        {
            assert!(
                res.is_err(),
                "Database should fail at {}: {:?}",
                path.display(),
                res
            );
        }
    }

    #[test]
    #[cfg(target_os = "linux")]
    fn linux_invalid_utf8_panicfree() {
        use std::ffi::OsStr;
        use std::os::unix::ffi::OsStrExt;

        // Here, the values 0x66 and 0x6f correspond to 'f' and 'o'
        // respectively. The value 0x80 is a lone continuation byte, invalid
        // in a UTF-8 sequence.
        let source = [0x66, 0x6f, 0x80, 0x6f];
        let os_str = OsStr::from_bytes(&source[..]);
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false);
        assert!(
            res.is_ok(),
            "Database should not fail at {}: {:?}",
            path.display(),
            res
        );
    }

    #[test]
    #[cfg(target_os = "macos")]
    fn macos_invalid_utf8_panicfree() {
        use std::ffi::OsStr;
        use std::os::unix::ffi::OsStrExt;

        // Here, the values 0x66 and 0x6f correspond to 'f' and 'o'
        // respectively. The value 0x80 is a lone continuation byte, invalid
        // in a UTF-8 sequence.
        let source = [0x66, 0x6f, 0x80, 0x6f];
        let os_str = OsStr::from_bytes(&source[..]);
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false);
        assert!(
            res.is_err(),
            "Database should not fail at {}: {:?}",
            path.display(),
            res
        );
    }

    #[test]
    fn test_data_dir_rkv_inits() {
        let dir = tempdir().unwrap();
        Database::new(dir.path(), false).unwrap();

        assert!(dir.path().exists());
    }

    #[test]
    fn test_ping_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false).unwrap();

        assert!(db.ping_lifetime_data.is_none());

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
        assert_eq!(1, found_metrics, "We only expect 1 Lifetime.Ping metric.");
    }

    #[test]
    fn test_application_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false).unwrap();

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage1";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::Application,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);
        assert_eq!(
            1, found_metrics,
            "We only expect 1 Lifetime.Application metric."
        );
    }

    #[test]
    fn test_user_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false).unwrap();

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage2";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::User,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
        assert_eq!(1, found_metrics, "We only expect 1 Lifetime.User metric.");
    }

    #[test]
    fn test_clear_ping_storage() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false).unwrap();

        // Attempt to record a known value for every single lifetime.
        let test_storage = "test-storage";
        db.record_per_lifetime(
            Lifetime::User,
            test_storage,
            "telemetry_test.test_name_user",
            &Metric::String("test-value-user".to_string()),
        )
        .unwrap();
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            "telemetry_test.test_name_ping",
            &Metric::String("test-value-ping".to_string()),
        )
        .unwrap();
        db.record_per_lifetime(
            Lifetime::Application,
            test_storage,
            "telemetry_test.test_name_application",
            &Metric::String("test-value-application".to_string()),
        )
        .unwrap();

        // Take a snapshot for the data, all the lifetimes.
        {
            let mut snapshot: HashMap<String, String> = HashMap::new();
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                match metric {
                    Metric::String(s) => snapshot.insert(metric_id, s.to_string()),
                    _ => panic!("Unexpected data found"),
                };
            };

            db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);

            assert_eq!(3, snapshot.len(), "We expect all lifetimes to be present.");
            assert!(snapshot.contains_key("telemetry_test.test_name_user"));
            assert!(snapshot.contains_key("telemetry_test.test_name_ping"));
            assert!(snapshot.contains_key("telemetry_test.test_name_application"));
        }

        // Clear the Ping lifetime.
        db.clear_ping_lifetime_storage(test_storage).unwrap();

        // Take a snapshot again and check that we're only clearing the Ping lifetime.
        {
            let mut snapshot: HashMap<String, String> = HashMap::new();
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                match metric {
                    Metric::String(s) => snapshot.insert(metric_id, s.to_string()),
                    _ => panic!("Unexpected data found"),
                };
            };

            db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);

            assert_eq!(2, snapshot.len(), "We only expect 2 metrics to be left.");
            assert!(snapshot.contains_key("telemetry_test.test_name_user"));
            assert!(snapshot.contains_key("telemetry_test.test_name_application"));
        }
    }

    #[test]
    fn test_remove_single_metric() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false).unwrap();

        let test_storage = "test-storage-single-lifetime";
        let metric_id_pattern = "telemetry_test.single_metric";

        // Write sample metrics to the database.
        let lifetimes = vec![Lifetime::User, Lifetime::Ping, Lifetime::Application];

        for lifetime in lifetimes.iter() {
            for value in &["retain", "delete"] {
                db.record_per_lifetime(
                    *lifetime,
                    test_storage,
                    &format!("{}_{}", metric_id_pattern, value),
                    &Metric::String((*value).to_string()),
                )
                .unwrap();
            }
        }

        // Remove "telemetry_test.single_metric_delete" from each lifetime.
        for lifetime in lifetimes.iter() {
            db.remove_single_metric(
                *lifetime,
                test_storage,
                &format!("{}_delete", metric_id_pattern),
            )
            .unwrap();
        }

        // Verify that "telemetry_test.single_metric_retain" is still around for all lifetimes.
        for lifetime in lifetimes.iter() {
            let mut found_metrics = 0;
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                found_metrics += 1;
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                assert_eq!(format!("{}_retain", metric_id_pattern), metric_id);
                match metric {
                    Metric::String(s) => assert_eq!("retain", s),
                    _ => panic!("Unexpected data found"),
                }
            };

            // Check the User lifetime.
            db.iter_store_from(*lifetime, test_storage, None, &mut snapshotter);
            assert_eq!(
                1, found_metrics,
                "We only expect 1 metric for this lifetime."
            );
        }
    }

    #[test]
    fn test_delayed_ping_lifetime_persistence() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), true).unwrap();
        let test_storage = "test-storage";

        assert!(db.ping_lifetime_data.is_some());

        // Attempt to record a known value.
        let test_value1 = "test-value1";
        let test_metric_id1 = "telemetry_test.test_name1";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id1,
            &Metric::String(test_value1.to_string()),
        )
        .unwrap();

        // Attempt to persist data.
        db.persist_ping_lifetime_data().unwrap();

        // Attempt to record another known value.
        let test_value2 = "test-value2";
        let test_metric_id2 = "telemetry_test.test_name2";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id2,
            &Metric::String(test_value2.to_string()),
        )
        .unwrap();

        {
            // At this stage we expect `test_value1` to be persisted and in memory,
            // since it was recorded before calling `persist_ping_lifetime_data`,
            // and `test_value2` to be only in memory, since it was recorded after.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();

            // Verify that test_value1 is in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id1))
                .unwrap_or(None)
                .is_some());
            // Verifiy that test_value2 is **not** in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id2))
                .unwrap_or(None)
                .is_none());

            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
            // Verify that test_value2 is in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id2))
                .is_some());
        }

        // Attempt to persist data again.
        db.persist_ping_lifetime_data().unwrap();

        {
            // At this stage we expect `test_value1` and `test_value2` to
            // be persisted, since both were created before a call to
            // `persist_ping_lifetime_data`.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();

            // Verify that test_value1 is in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id1))
                .unwrap_or(None)
                .is_some());
            // Verifiy that test_value2 is also in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id2))
                .unwrap_or(None)
                .is_some());

            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
            // Verify that test_value2 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id2))
                .is_some());
        }
    }

    #[test]
    fn test_load_ping_lifetime_data_from_memory() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();

        let test_storage = "test-storage";
        let test_value = "test-value";
        let test_metric_id = "telemetry_test.test_name";

        {
            let db = Database::new(dir.path(), true).unwrap();

            // Attempt to record a known value.
            db.record_per_lifetime(
                Lifetime::Ping,
                test_storage,
                test_metric_id,
                &Metric::String(test_value.to_string()),
            )
            .unwrap();

            // Verify that test_value is in memory.
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id))
                .is_some());

            // Attempt to persist data.
            db.persist_ping_lifetime_data().unwrap();

            // Verify that test_value is now in rkv.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id))
                .unwrap_or(None)
                .is_some());
        }

        // Now create a new instace of the db and check if data was
        // correctly loaded from rkv to memory.
        {
            let db = Database::new(dir.path(), true).unwrap();

            // Verify that test_value is in memory.
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id))
                .is_some());

            // Verify that test_value is also in rkv.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id))
                .unwrap_or(None)
                .is_some());
        }
    }

    #[test]
    fn doesnt_record_when_upload_is_disabled() {
        let (mut glean, dir) = new_glean(None);

        // Init the database in a temporary directory.

        let test_storage = "test-storage";
        let test_data = CommonMetricData::new("category", "name", test_storage);
        let test_metric_id = test_data.identifier(&glean);

        // Attempt to record metric with the record and record_with functions,
        // this should work since upload is enabled.
        let db = Database::new(dir.path(), true).unwrap();
        db.record(&glean, &test_data, &Metric::String("record".to_owned()));
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );

        db.record_with(&glean, &test_data, |_| {
            Metric::String("record_with".to_owned())
        });
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );

        // Disable upload
        glean.set_upload_enabled(false);

        // Attempt to record metric with the record and record_with functions,
        // this should work since upload is now **disabled**.
        db.record(&glean, &test_data, &Metric::String("record_nop".to_owned()));
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );
        db.record_with(&glean, &test_data, |_| {
            Metric::String("record_with_nop".to_owned())
        });
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );
    }

    /// LDMB ignores an empty database file just fine.
    #[cfg(not(feature = "rkv-safe-mode"))]
    #[test]
    fn empty_data_file() {
        let dir = tempdir().unwrap();

        // Create database directory structure.
        let database_dir = dir.path().join("db");
        fs::create_dir_all(&database_dir).expect("create database dir");

        // Create empty database file.
        let datamdb = database_dir.join("data.mdb");
        let f = fs::File::create(datamdb).expect("create database file");
        drop(f);

        Database::new(dir.path(), false).unwrap();

        assert!(dir.path().exists());
    }

    #[cfg(feature = "rkv-safe-mode")]
    mod safe_mode {
        use std::fs::File;

        use super::*;
        use rkv::Value;

        #[test]
        fn empty_data_file() {
            let dir = tempdir().unwrap();

            // Create database directory structure.
            let database_dir = dir.path().join("db");
            fs::create_dir_all(&database_dir).expect("create database dir");

            // Create empty database file.
            let safebin = database_dir.join("data.safe.bin");
            let f = File::create(safebin).expect("create database file");
            drop(f);

            Database::new(dir.path(), false).unwrap();

            assert!(dir.path().exists());
        }

        #[test]
        fn corrupted_data_file() {
            let dir = tempdir().unwrap();

            // Create database directory structure.
            let database_dir = dir.path().join("db");
            fs::create_dir_all(&database_dir).expect("create database dir");

            // Create empty database file.
            let safebin = database_dir.join("data.safe.bin");
            fs::write(safebin, "<broken>").expect("write to database file");

            Database::new(dir.path(), false).unwrap();

            assert!(dir.path().exists());
        }

        #[test]
        fn migration_works_on_startup() {
            let dir = tempdir().unwrap();

            let database_dir = dir.path().join("db");
            let datamdb = database_dir.join("data.mdb");
            let lockmdb = database_dir.join("lock.mdb");
            let safebin = database_dir.join("data.safe.bin");

            assert!(!safebin.exists());
            assert!(!datamdb.exists());
            assert!(!lockmdb.exists());

            let store_name = "store1";
            let metric_name = "bool";
            let key = Database::get_storage_key(store_name, Some(metric_name));

            // Ensure some old data in the LMDB format exists.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                let rkv_db = rkv::Rkv::new::<rkv::backend::Lmdb>(&database_dir).expect("rkv env");

                let store = rkv_db
                    .open_single("ping", StoreOptions::create())
                    .expect("opened");
                let mut writer = rkv_db.write().expect("writer");
                let metric = Metric::Boolean(true);
                let value = bincode::serialize(&metric).expect("serialized");
                store
                    .put(&mut writer, &key, &Value::Blob(&value))
                    .expect("wrote");
                writer.commit().expect("committed");

                assert!(datamdb.exists());
                assert!(lockmdb.exists());
                assert!(!safebin.exists());
            }

            // First open should migrate the data.
            {
                let db = Database::new(dir.path(), false).unwrap();
                let safebin = database_dir.join("data.safe.bin");
                assert!(safebin.exists(), "safe-mode file should exist");
                assert!(!datamdb.exists(), "LMDB data should be deleted");
                assert!(!lockmdb.exists(), "LMDB lock should be deleted");

                let mut stored_metrics = vec![];
                let mut snapshotter = |name: &[u8], metric: &Metric| {
                    let name = str::from_utf8(name).unwrap().to_string();
                    stored_metrics.push((name, metric.clone()))
                };
                db.iter_store_from(Lifetime::Ping, "store1", None, &mut snapshotter);

                assert_eq!(1, stored_metrics.len());
                assert_eq!(metric_name, stored_metrics[0].0);
                assert_eq!(&Metric::Boolean(true), &stored_metrics[0].1);
            }

            // Next open should not re-create the LMDB files.
            {
                let db = Database::new(dir.path(), false).unwrap();
                let safebin = database_dir.join("data.safe.bin");
                assert!(safebin.exists(), "safe-mode file exists");
                assert!(!datamdb.exists(), "LMDB data should not be recreated");
                assert!(!lockmdb.exists(), "LMDB lock should not be recreated");

                let mut stored_metrics = vec![];
                let mut snapshotter = |name: &[u8], metric: &Metric| {
                    let name = str::from_utf8(name).unwrap().to_string();
                    stored_metrics.push((name, metric.clone()))
                };
                db.iter_store_from(Lifetime::Ping, "store1", None, &mut snapshotter);

                assert_eq!(1, stored_metrics.len());
                assert_eq!(metric_name, stored_metrics[0].0);
                assert_eq!(&Metric::Boolean(true), &stored_metrics[0].1);
            }
        }

        #[test]
        fn migration_doesnt_overwrite() {
            let dir = tempdir().unwrap();

            let database_dir = dir.path().join("db");
            let datamdb = database_dir.join("data.mdb");
            let lockmdb = database_dir.join("lock.mdb");
            let safebin = database_dir.join("data.safe.bin");

            assert!(!safebin.exists());
            assert!(!datamdb.exists());
            assert!(!lockmdb.exists());

            let store_name = "store1";
            let metric_name = "counter";
            let key = Database::get_storage_key(store_name, Some(metric_name));

            // Ensure some old data in the LMDB format exists.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                let rkv_db = rkv::Rkv::new::<rkv::backend::Lmdb>(&database_dir).expect("rkv env");

                let store = rkv_db
                    .open_single("ping", StoreOptions::create())
                    .expect("opened");
                let mut writer = rkv_db.write().expect("writer");
                let metric = Metric::Counter(734); // this value will be ignored
                let value = bincode::serialize(&metric).expect("serialized");
                store
                    .put(&mut writer, &key, &Value::Blob(&value))
                    .expect("wrote");
                writer.commit().expect("committed");

                assert!(datamdb.exists());
                assert!(lockmdb.exists());
            }

            // Ensure some data exists in the new database.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                let rkv_db =
                    rkv::Rkv::new::<rkv::backend::SafeMode>(&database_dir).expect("rkv env");

                let store = rkv_db
                    .open_single("ping", StoreOptions::create())
                    .expect("opened");
                let mut writer = rkv_db.write().expect("writer");
                let metric = Metric::Counter(2);
                let value = bincode::serialize(&metric).expect("serialized");
                store
                    .put(&mut writer, &key, &Value::Blob(&value))
                    .expect("wrote");
                writer.commit().expect("committed");

                assert!(safebin.exists());
            }

            // First open should try migration and ignore it, because destination is not empty.
            // It also deletes the leftover LMDB database.
            {
                let db = Database::new(dir.path(), false).unwrap();
                let safebin = database_dir.join("data.safe.bin");
                assert!(safebin.exists(), "safe-mode file should exist");
                assert!(!datamdb.exists(), "LMDB data should be deleted");
                assert!(!lockmdb.exists(), "LMDB lock should be deleted");

                let mut stored_metrics = vec![];
                let mut snapshotter = |name: &[u8], metric: &Metric| {
                    let name = str::from_utf8(name).unwrap().to_string();
                    stored_metrics.push((name, metric.clone()))
                };
                db.iter_store_from(Lifetime::Ping, "store1", None, &mut snapshotter);

                assert_eq!(1, stored_metrics.len());
                assert_eq!(metric_name, stored_metrics[0].0);
                assert_eq!(&Metric::Counter(2), &stored_metrics[0].1);
            }
        }

        #[test]
        fn migration_ignores_broken_database() {
            let dir = tempdir().unwrap();

            let database_dir = dir.path().join("db");
            let datamdb = database_dir.join("data.mdb");
            let lockmdb = database_dir.join("lock.mdb");
            let safebin = database_dir.join("data.safe.bin");

            assert!(!safebin.exists());
            assert!(!datamdb.exists());
            assert!(!lockmdb.exists());

            let store_name = "store1";
            let metric_name = "counter";
            let key = Database::get_storage_key(store_name, Some(metric_name));

            // Ensure some old data in the LMDB format exists.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                fs::write(&datamdb, "bogus").expect("dbfile created");

                assert!(datamdb.exists());
            }

            // Ensure some data exists in the new database.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                let rkv_db =
                    rkv::Rkv::new::<rkv::backend::SafeMode>(&database_dir).expect("rkv env");

                let store = rkv_db
                    .open_single("ping", StoreOptions::create())
                    .expect("opened");
                let mut writer = rkv_db.write().expect("writer");
                let metric = Metric::Counter(2);
                let value = bincode::serialize(&metric).expect("serialized");
                store
                    .put(&mut writer, &key, &Value::Blob(&value))
                    .expect("wrote");
                writer.commit().expect("committed");
            }

            // First open should try migration and ignore it, because destination is not empty.
            // It also deletes the leftover LMDB database.
            {
                let db = Database::new(dir.path(), false).unwrap();
                let safebin = database_dir.join("data.safe.bin");
                assert!(safebin.exists(), "safe-mode file should exist");
                assert!(!datamdb.exists(), "LMDB data should be deleted");
                assert!(!lockmdb.exists(), "LMDB lock should be deleted");

                let mut stored_metrics = vec![];
                let mut snapshotter = |name: &[u8], metric: &Metric| {
                    let name = str::from_utf8(name).unwrap().to_string();
                    stored_metrics.push((name, metric.clone()))
                };
                db.iter_store_from(Lifetime::Ping, "store1", None, &mut snapshotter);

                assert_eq!(1, stored_metrics.len());
                assert_eq!(metric_name, stored_metrics[0].0);
                assert_eq!(&Metric::Counter(2), &stored_metrics[0].1);
            }
        }

        #[test]
        fn migration_ignores_empty_database() {
            let dir = tempdir().unwrap();

            let database_dir = dir.path().join("db");
            let datamdb = database_dir.join("data.mdb");
            let lockmdb = database_dir.join("lock.mdb");
            let safebin = database_dir.join("data.safe.bin");

            assert!(!safebin.exists());
            assert!(!datamdb.exists());
            assert!(!lockmdb.exists());

            // Ensure old LMDB database exists, but is empty.
            {
                fs::create_dir_all(&database_dir).expect("create dir");
                let rkv_db = rkv::Rkv::new::<rkv::backend::Lmdb>(&database_dir).expect("rkv env");
                drop(rkv_db);
                assert!(datamdb.exists());
                assert!(lockmdb.exists());
            }

            // First open should try migration, but find no data.
            // safe-mode does not write an empty database to disk.
            // It also deletes the leftover LMDB database.
            {
                let _db = Database::new(dir.path(), false).unwrap();
                let safebin = database_dir.join("data.safe.bin");
                assert!(!safebin.exists(), "safe-mode file should exist");
                assert!(!datamdb.exists(), "LMDB data should be deleted");
                assert!(!lockmdb.exists(), "LMDB lock should be deleted");
            }
        }
    }
}
