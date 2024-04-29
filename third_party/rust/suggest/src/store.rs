/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use std::{
    collections::{BTreeMap, BTreeSet},
    path::{Path, PathBuf},
    sync::Arc,
};

use error_support::handle_error;
use once_cell::sync::OnceCell;
use parking_lot::Mutex;
use remote_settings::{
    self, GetItemsOptions, RemoteSettingsConfig, RemoteSettingsRecord, RemoteSettingsServer,
    SortOrder,
};
use rusqlite::{
    types::{FromSql, ToSqlOutput},
    ToSql,
};
use serde::{de::DeserializeOwned, Deserialize, Serialize};

use crate::{
    config::{SuggestGlobalConfig, SuggestProviderConfig},
    db::{
        ConnectionType, SuggestDao, SuggestDb, LAST_INGEST_META_UNPARSABLE,
        UNPARSABLE_RECORDS_META_KEY,
    },
    error::Error,
    provider::SuggestionProvider,
    rs::{
        SuggestAttachment, SuggestRecord, SuggestRecordId, SuggestRecordType,
        SuggestRemoteSettingsClient, DEFAULT_RECORDS_TYPES, REMOTE_SETTINGS_COLLECTION,
        SUGGESTIONS_PER_ATTACHMENT,
    },
    schema::VERSION,
    Result, SuggestApiResult, Suggestion, SuggestionQuery,
};

/// The chunk size used to request unparsable records.
pub const UNPARSABLE_IDS_PER_REQUEST: usize = 150;

/// Builder for [SuggestStore]
///
/// Using a builder is preferred to calling the constructor directly since it's harder to confuse
/// the data_path and cache_path strings.
pub struct SuggestStoreBuilder(Mutex<SuggestStoreBuilderInner>);

#[derive(Default)]
struct SuggestStoreBuilderInner {
    data_path: Option<String>,
    remote_settings_server: Option<RemoteSettingsServer>,
    remote_settings_config: Option<RemoteSettingsConfig>,
}

impl Default for SuggestStoreBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl SuggestStoreBuilder {
    pub fn new() -> SuggestStoreBuilder {
        Self(Mutex::new(SuggestStoreBuilderInner::default()))
    }

    pub fn data_path(self: Arc<Self>, path: String) -> Arc<Self> {
        self.0.lock().data_path = Some(path);
        self
    }

    pub fn cache_path(self: Arc<Self>, _path: String) -> Arc<Self> {
        // We used to use this, but we're not using it anymore, just ignore the call
        self
    }

    pub fn remote_settings_config(self: Arc<Self>, config: RemoteSettingsConfig) -> Arc<Self> {
        self.0.lock().remote_settings_config = Some(config);
        self
    }

    pub fn remote_settings_server(self: Arc<Self>, server: RemoteSettingsServer) -> Arc<Self> {
        self.0.lock().remote_settings_server = Some(server);
        self
    }

    #[handle_error(Error)]
    pub fn build(&self) -> SuggestApiResult<Arc<SuggestStore>> {
        let inner = self.0.lock();
        let data_path = inner
            .data_path
            .clone()
            .ok_or_else(|| Error::SuggestStoreBuilder("data_path not specified".to_owned()))?;
        let remote_settings_config = match (
            inner.remote_settings_server.as_ref(),
            inner.remote_settings_config.as_ref(),
        ) {
            (Some(server), None) => RemoteSettingsConfig {
                server: Some(server.clone()),
                server_url: None,
                bucket_name: None,
                collection_name: REMOTE_SETTINGS_COLLECTION.into(),
            },
            (None, Some(remote_settings_config)) => remote_settings_config.clone(),
            (None, None) => RemoteSettingsConfig {
                server: None,
                server_url: None,
                bucket_name: None,
                collection_name: REMOTE_SETTINGS_COLLECTION.into(),
            },
            (Some(_), Some(_)) => Err(Error::SuggestStoreBuilder(
                "can't specify both `remote_settings_server` and `remote_settings_config`"
                    .to_owned(),
            ))?,
        };
        let settings_client = remote_settings::Client::new(remote_settings_config)?;
        Ok(Arc::new(SuggestStore {
            inner: SuggestStoreInner::new(data_path, settings_client),
        }))
    }
}

/// The store is the entry point to the Suggest component. It incrementally
/// downloads suggestions from the Remote Settings service, stores them in a
/// local database, and returns them in response to user queries.
///
/// Your application should create a single store, and manage it as a singleton.
/// The store is thread-safe, and supports concurrent queries and ingests. We
/// expect that your application will call [`SuggestStore::query()`] to show
/// suggestions as the user types into the address bar, and periodically call
/// [`SuggestStore::ingest()`] in the background to update the database with
/// new suggestions from Remote Settings.
///
/// For responsiveness, we recommend always calling `query()` on a worker
/// thread. When the user types new input into the address bar, call
/// [`SuggestStore::interrupt()`] on the main thread to cancel the query
/// for the old input, and unblock the worker thread for the new query.
///
/// The store keeps track of the state needed to support incremental ingestion,
/// but doesn't schedule the ingestion work itself, or decide how many
/// suggestions to ingest at once. This is for two reasons:
///
/// 1. The primitives for scheduling background work vary between platforms, and
///    aren't available to the lower-level Rust layer. You might use an idle
///    timer on Desktop, `WorkManager` on Android, or `BGTaskScheduler` on iOS.
/// 2. Ingestion constraints can change, depending on the platform and the needs
///    of your application. A mobile device on a metered connection might want
///    to request a small subset of the Suggest data and download the rest
///    later, while a desktop on a fast link might download the entire dataset
///    on the first launch.
pub struct SuggestStore {
    inner: SuggestStoreInner<remote_settings::Client>,
}

/// For records that aren't currently parsable,
/// the record ID and the schema version it's first seen in
/// is recorded in the meta table using `UNPARSABLE_RECORDS_META_KEY` as its key.
/// On the first ingest after an upgrade, re-request those records from Remote Settings,
/// and try to ingest them again.
#[derive(Deserialize, Serialize, Default, Debug)]
#[serde(transparent)]
pub(crate) struct UnparsableRecords(pub BTreeMap<String, UnparsableRecord>);

impl FromSql for UnparsableRecords {
    fn column_result(value: rusqlite::types::ValueRef<'_>) -> rusqlite::types::FromSqlResult<Self> {
        serde_json::from_str(value.as_str()?)
            .map_err(|err| rusqlite::types::FromSqlError::Other(Box::new(err)))
    }
}

impl ToSql for UnparsableRecords {
    fn to_sql(&self) -> rusqlite::Result<rusqlite::types::ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(serde_json::to_string(self).map_err(
            |err| rusqlite::Error::ToSqlConversionFailure(Box::new(err)),
        )?))
    }
}

#[derive(Deserialize, Serialize, Debug)]
pub(crate) struct UnparsableRecord {
    #[serde(rename = "v")]
    pub schema_version: u32,
}

impl SuggestStore {
    /// Creates a Suggest store.
    #[handle_error(Error)]
    pub fn new(
        path: &str,
        settings_config: Option<RemoteSettingsConfig>,
    ) -> SuggestApiResult<Self> {
        let settings_client = || -> Result<_> {
            Ok(remote_settings::Client::new(
                settings_config.unwrap_or_else(|| RemoteSettingsConfig {
                    server: None,
                    server_url: None,
                    bucket_name: None,
                    collection_name: REMOTE_SETTINGS_COLLECTION.into(),
                }),
            )?)
        }()?;
        Ok(Self {
            inner: SuggestStoreInner::new(path.to_owned(), settings_client),
        })
    }

    /// Queries the database for suggestions.
    #[handle_error(Error)]
    pub fn query(&self, query: SuggestionQuery) -> SuggestApiResult<Vec<Suggestion>> {
        self.inner.query(query)
    }

    /// Dismiss a suggestion
    ///
    /// Dismissed suggestions will not be returned again
    ///
    /// In the case of AMP suggestions this should be the raw URL.
    #[handle_error(Error)]
    pub fn dismiss_suggestion(&self, suggestion_url: String) -> SuggestApiResult<()> {
        self.inner.dismiss_suggestion(suggestion_url)
    }

    /// Clear dismissed suggestions
    #[handle_error(Error)]
    pub fn clear_dismissed_suggestions(&self) -> SuggestApiResult<()> {
        self.inner.clear_dismissed_suggestions()
    }

    /// Interrupts any ongoing queries.
    ///
    /// This should be called when the user types new input into the address
    /// bar, to ensure that they see fresh suggestions as they type. This
    /// method does not interrupt any ongoing ingests.
    pub fn interrupt(&self) {
        self.inner.interrupt()
    }

    /// Ingests new suggestions from Remote Settings.
    #[handle_error(Error)]
    pub fn ingest(&self, constraints: SuggestIngestionConstraints) -> SuggestApiResult<()> {
        self.inner.ingest(constraints)
    }

    /// Removes all content from the database.
    #[handle_error(Error)]
    pub fn clear(&self) -> SuggestApiResult<()> {
        self.inner.clear()
    }

    // Returns global Suggest configuration data.
    #[handle_error(Error)]
    pub fn fetch_global_config(&self) -> SuggestApiResult<SuggestGlobalConfig> {
        self.inner.fetch_global_config()
    }

    // Returns per-provider Suggest configuration data.
    #[handle_error(Error)]
    pub fn fetch_provider_config(
        &self,
        provider: SuggestionProvider,
    ) -> SuggestApiResult<Option<SuggestProviderConfig>> {
        self.inner.fetch_provider_config(provider)
    }
}

/// Constraints limit which suggestions to ingest from Remote Settings.
#[derive(Clone, Default, Debug)]
pub struct SuggestIngestionConstraints {
    /// The approximate maximum number of suggestions to ingest. Set to [`None`]
    /// for "no limit".
    ///
    /// Because of how suggestions are partitioned in Remote Settings, this is a
    /// soft limit, and the store might ingest more than requested.
    pub max_suggestions: Option<u64>,
    pub providers: Option<Vec<SuggestionProvider>>,
}

/// The implementation of the store. This is generic over the Remote Settings
/// client, and is split out from the concrete [`SuggestStore`] for testing
/// with a mock client.
pub(crate) struct SuggestStoreInner<S> {
    /// Path to the persistent SQL database.
    ///
    /// This stores things that should persist when the user clears their cache.
    /// It's not currently used because not all consumers pass this in yet.
    #[allow(unused)]
    data_path: PathBuf,
    dbs: OnceCell<SuggestStoreDbs>,
    settings_client: S,
}

impl<S> SuggestStoreInner<S> {
    pub fn new(data_path: impl Into<PathBuf>, settings_client: S) -> Self {
        Self {
            data_path: data_path.into(),
            dbs: OnceCell::new(),
            settings_client,
        }
    }

    /// Returns this store's database connections, initializing them if
    /// they're not already open.
    fn dbs(&self) -> Result<&SuggestStoreDbs> {
        self.dbs
            .get_or_try_init(|| SuggestStoreDbs::open(&self.data_path))
    }

    fn query(&self, query: SuggestionQuery) -> Result<Vec<Suggestion>> {
        if query.keyword.is_empty() || query.providers.is_empty() {
            return Ok(Vec::new());
        }
        self.dbs()?.reader.read(|dao| dao.fetch_suggestions(&query))
    }

    fn dismiss_suggestion(&self, suggestion_url: String) -> Result<()> {
        self.dbs()?
            .writer
            .write(|dao| dao.insert_dismissal(&suggestion_url))
    }

    fn clear_dismissed_suggestions(&self) -> Result<()> {
        self.dbs()?.writer.write(|dao| dao.clear_dismissals())?;
        Ok(())
    }

    fn interrupt(&self) {
        if let Some(dbs) = self.dbs.get() {
            // Only interrupt if the databases are already open.
            dbs.reader.interrupt_handle.interrupt();
        }
    }

    fn clear(&self) -> Result<()> {
        self.dbs()?.writer.write(|dao| dao.clear())
    }

    pub fn fetch_global_config(&self) -> Result<SuggestGlobalConfig> {
        self.dbs()?.reader.read(|dao| dao.get_global_config())
    }

    pub fn fetch_provider_config(
        &self,
        provider: SuggestionProvider,
    ) -> Result<Option<SuggestProviderConfig>> {
        self.dbs()?
            .reader
            .read(|dao| dao.get_provider_config(provider))
    }
}

impl<S> SuggestStoreInner<S>
where
    S: SuggestRemoteSettingsClient,
{
    pub fn ingest(&self, constraints: SuggestIngestionConstraints) -> Result<()> {
        let writer = &self.dbs()?.writer;

        if let Some(unparsable_records) =
            writer.read(|dao| dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY))?
        {
            let all_unparsable_ids = unparsable_records
                .0
                .iter()
                .filter(|(_, unparsable_record)| unparsable_record.schema_version < VERSION)
                .map(|(record_id, _)| record_id)
                .collect::<Vec<_>>();
            for unparsable_ids in all_unparsable_ids.chunks(UNPARSABLE_IDS_PER_REQUEST) {
                let mut options = GetItemsOptions::new();
                for unparsable_id in unparsable_ids {
                    options.filter_eq("id", *unparsable_id);
                }
                let records_chunk = self
                    .settings_client
                    .get_records_with_options(&options)?
                    .records;

                self.ingest_records(LAST_INGEST_META_UNPARSABLE, writer, &records_chunk)?;
            }
        }

        // use std::collections::BTreeSet;
        let ingest_record_types = if let Some(rt) = &constraints.providers {
            rt.iter()
                .flat_map(|x| x.records_for_provider())
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect()
        } else {
            DEFAULT_RECORDS_TYPES.to_vec()
        };

        for ingest_record_type in ingest_record_types {
            self.ingest_records_by_type(ingest_record_type, writer, &constraints)?;
        }

        Ok(())
    }

    fn ingest_records_by_type(
        &self,
        ingest_record_type: SuggestRecordType,
        writer: &SuggestDb,
        constraints: &SuggestIngestionConstraints,
    ) -> Result<()> {
        let mut options = GetItemsOptions::new();

        // Remote Settings returns records in descending modification order
        // (newest first), but we want them in ascending order (oldest first),
        // so that we can eventually resume downloading where we left off.
        options.sort("last_modified", SortOrder::Ascending);

        options.filter_eq("type", ingest_record_type.to_string());

        // Get the last ingest value. This is the max of the last_ingest_keys
        // that are in the database.
        if let Some(last_ingest) = writer
            .read(|dao| dao.get_meta::<u64>(ingest_record_type.last_ingest_meta_key().as_str()))?
        {
            // Only download changes since our last ingest. If our last ingest
            // was interrupted, we'll pick up where we left off.
            options.filter_gt("last_modified", last_ingest.to_string());
        }

        if let Some(max_suggestions) = constraints.max_suggestions {
            // Each record's attachment has 200 suggestions, so download enough
            // records to cover the requested maximum.
            let max_records = (max_suggestions.saturating_sub(1) / SUGGESTIONS_PER_ATTACHMENT) + 1;
            options.limit(max_records);
        }

        let records = self
            .settings_client
            .get_records_with_options(&options)?
            .records;
        self.ingest_records(&ingest_record_type.last_ingest_meta_key(), writer, &records)?;
        Ok(())
    }

    fn ingest_records(
        &self,
        last_ingest_key: &str,
        writer: &SuggestDb,
        records: &[RemoteSettingsRecord],
    ) -> Result<()> {
        for record in records {
            let record_id = SuggestRecordId::from(&record.id);
            if record.deleted {
                // If the entire record was deleted, drop all its suggestions
                // and advance the last ingest time.
                writer.write(|dao| dao.handle_deleted_record(last_ingest_key, record))?;
                continue;
            }
            let Ok(fields) =
                serde_json::from_value(serde_json::Value::Object(record.fields.clone()))
            else {
                // We don't recognize this record's type, so we don't know how
                // to ingest its suggestions. Skip processing this record.
                writer.write(|dao| dao.handle_unparsable_record(record))?;
                continue;
            };

            match fields {
                SuggestRecord::AmpWikipedia => {
                    self.ingest_attachment(
                        // TODO: Currently re-creating the last_ingest_key because using last_ingest_meta
                        // breaks the tests (particularly the unparsable functionality). So, keeping
                        // a direct reference until we remove the "unparsable" functionality.
                        &SuggestRecordType::AmpWikipedia.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amp_wikipedia_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::AmpMobile => {
                    self.ingest_attachment(
                        &SuggestRecordType::AmpMobile.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amp_mobile_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Icon => {
                    let (Some(icon_id), Some(attachment)) =
                        (record_id.as_icon_id(), record.attachment.as_ref())
                    else {
                        // An icon record should have an icon ID and an
                        // attachment. Icons that don't have these are
                        // malformed, so skip to the next record.
                        writer.write(|dao| {
                            dao.put_last_ingest_if_newer(
                                &SuggestRecordType::Icon.last_ingest_meta_key(),
                                record.last_modified,
                            )
                        })?;
                        continue;
                    };
                    let data = self.settings_client.get_attachment(&attachment.location)?;
                    writer.write(|dao| {
                        dao.put_icon(icon_id, &data, &attachment.mimetype)?;
                        dao.handle_ingested_record(
                            &SuggestRecordType::Icon.last_ingest_meta_key(),
                            record,
                        )
                    })?;
                }
                SuggestRecord::Amo => {
                    self.ingest_attachment(
                        &SuggestRecordType::Amo.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amo_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Pocket => {
                    self.ingest_attachment(
                        &SuggestRecordType::Pocket.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_pocket_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Yelp => {
                    self.ingest_attachment(
                        &SuggestRecordType::Yelp.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| match suggestions.first() {
                            Some(suggestion) => dao.insert_yelp_suggestions(record_id, suggestion),
                            None => Ok(()),
                        },
                    )?;
                }
                SuggestRecord::Mdn => {
                    self.ingest_attachment(
                        &SuggestRecordType::Mdn.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_mdn_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Weather(data) => {
                    self.ingest_record(
                        &SuggestRecordType::Weather.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, record_id| dao.insert_weather_data(record_id, &data),
                    )?;
                }
                SuggestRecord::GlobalConfig(config) => {
                    self.ingest_record(
                        &SuggestRecordType::GlobalConfig.last_ingest_meta_key(),
                        writer,
                        record,
                        |dao, _| dao.put_global_config(&SuggestGlobalConfig::from(&config)),
                    )?;
                }
            }
        }
        Ok(())
    }

    fn ingest_record(
        &self,
        last_ingest_key: &str,
        writer: &SuggestDb,
        record: &RemoteSettingsRecord,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId) -> Result<()>,
    ) -> Result<()> {
        let record_id = SuggestRecordId::from(&record.id);

        writer.write(|dao| {
            // Drop any data that we previously ingested from this record.
            // Suggestions in particular don't have a stable identifier, and
            // determining which suggestions in the record actually changed is
            // more complicated than dropping and re-ingesting all of them.
            dao.drop_suggestions(&record_id)?;

            // Ingest (or re-ingest) all data in the record.
            ingestion_handler(dao, &record_id)?;

            dao.handle_ingested_record(last_ingest_key, record)
        })
    }

    fn ingest_attachment<T>(
        &self,
        last_ingest_key: &str,
        writer: &SuggestDb,
        record: &RemoteSettingsRecord,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId, &[T]) -> Result<()>,
    ) -> Result<()>
    where
        T: DeserializeOwned,
    {
        let Some(attachment) = record.attachment.as_ref() else {
            // This method should be called only when a record is expected to
            // have an attachment. If it doesn't have one, it's malformed, so
            // skip to the next record.
            writer
                .write(|dao| dao.put_last_ingest_if_newer(last_ingest_key, record.last_modified))?;
            return Ok(());
        };

        let attachment_data = self.settings_client.get_attachment(&attachment.location)?;
        match serde_json::from_slice::<SuggestAttachment<T>>(&attachment_data) {
            Ok(attachment) => {
                self.ingest_record(last_ingest_key, writer, record, |dao, record_id| {
                    ingestion_handler(dao, record_id, attachment.suggestions())
                })
            }
            Err(_) => writer.write(|dao| dao.handle_unparsable_record(record)),
        }
    }
}

#[cfg(feature = "benchmark_api")]
impl<S> SuggestStoreInner<S>
where
    S: SuggestRemoteSettingsClient,
{
    pub fn into_settings_client(self) -> S {
        self.settings_client
    }

    pub fn ensure_db_initialized(&self) {
        self.dbs().unwrap();
    }

    pub fn benchmark_ingest_records_by_type(&self, ingest_record_type: SuggestRecordType) {
        self.ingest_records_by_type(
            ingest_record_type,
            &self.dbs().unwrap().writer,
            &SuggestIngestionConstraints::default(),
        )
        .unwrap()
    }

    pub fn table_row_counts(&self) -> Vec<(String, u32)> {
        use sql_support::ConnExt;

        // Note: since this is just used for debugging, use unwrap to simplify the error handling.
        let reader = &self.dbs().unwrap().reader;
        let conn = reader.conn.lock();
        let table_names: Vec<String> = conn
            .query_rows_and_then(
                "SELECT name FROM sqlite_master where type = 'table'",
                (),
                |row| row.get(0),
            )
            .unwrap();
        let mut table_names_with_counts: Vec<(String, u32)> = table_names
            .into_iter()
            .map(|name| {
                let count: u32 = conn
                    .query_one(&format!("SELECT COUNT(*) FROM {name}"))
                    .unwrap();
                (name, count)
            })
            .collect();
        table_names_with_counts.sort_by(|a, b| (b.1.cmp(&a.1)));
        table_names_with_counts
    }
}

/// Holds a store's open connections to the Suggest database.
struct SuggestStoreDbs {
    /// A read-write connection used to update the database with new data.
    writer: SuggestDb,
    /// A read-only connection used to query the database.
    reader: SuggestDb,
}

impl SuggestStoreDbs {
    fn open(path: &Path) -> Result<Self> {
        // Order is important here: the writer must be opened first, so that it
        // can set up the database and run any migrations.
        let writer = SuggestDb::open(path, ConnectionType::ReadWrite)?;
        let reader = SuggestDb::open(path, ConnectionType::ReadOnly)?;
        Ok(Self { writer, reader })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::{cell::RefCell, collections::HashMap};

    use anyhow::{anyhow, Context};
    use expect_test::expect;
    use parking_lot::Once;
    use rc_crypto::rand;
    use remote_settings::{RemoteSettingsRecord, RemoteSettingsResponse};
    use serde_json::json;
    use sql_support::ConnExt;

    use crate::SuggestionProvider;

    /// Creates a unique in-memory Suggest store.
    fn unique_test_store<S>(settings_client: S) -> SuggestStoreInner<S>
    where
        S: SuggestRemoteSettingsClient,
    {
        let mut unique_suffix = [0u8; 8];
        rand::fill(&mut unique_suffix).expect("Failed to generate unique suffix for test store");
        // A store opens separate connections to the same database for reading
        // and writing, so we must give our in-memory database a name, and open
        // it in shared-cache mode so that both connections can access it.
        SuggestStoreInner::new(
            format!(
                "file:test_store_data_{}?mode=memory&cache=shared",
                hex::encode(unique_suffix),
            ),
            settings_client,
        )
    }

    /// A snapshot containing fake Remote Settings records and attachments for
    /// the store to ingest. We use snapshots to test the store's behavior in a
    /// data-driven way.
    struct Snapshot {
        records: Vec<RemoteSettingsRecord>,
        attachments: HashMap<&'static str, Vec<u8>>,
    }

    impl Snapshot {
        /// Creates a snapshot from a JSON value that represents a collection of
        /// Suggest Remote Settings records.
        ///
        /// You can use the [`serde_json::json!`] macro to construct the JSON
        /// value, then pass it to this function. It's easier to use the
        /// `Snapshot::with_records(json!(...))` idiom than to construct the
        /// records by hand.
        fn with_records(value: serde_json::Value) -> anyhow::Result<Self> {
            Ok(Self {
                records: serde_json::from_value(value)
                    .context("Couldn't create snapshot with Remote Settings records")?,
                attachments: HashMap::new(),
            })
        }

        /// Adds a data attachment with one or more suggestions to the snapshot.
        fn with_data(
            mut self,
            location: &'static str,
            value: serde_json::Value,
        ) -> anyhow::Result<Self> {
            self.attachments.insert(
                location,
                serde_json::to_vec(&value).context("Couldn't add data attachment to snapshot")?,
            );
            Ok(self)
        }

        /// Adds an icon attachment to the snapshot.
        fn with_icon(mut self, location: &'static str, bytes: Vec<u8>) -> Self {
            self.attachments.insert(location, bytes);
            self
        }
    }

    /// A fake Remote Settings client that returns records and attachments from
    /// a snapshot.
    struct SnapshotSettingsClient {
        /// The current snapshot. You can modify it using
        /// [`RefCell::borrow_mut()`] to simulate remote updates in tests.
        snapshot: RefCell<Snapshot>,

        /// The options passed to the last [`Self::get_records_with_options()`]
        /// call.
        last_get_records_options: RefCell<Option<GetItemsOptions>>,
    }

    impl SnapshotSettingsClient {
        /// Creates a client with an initial snapshot.
        fn with_snapshot(snapshot: Snapshot) -> Self {
            Self {
                snapshot: RefCell::new(snapshot),
                last_get_records_options: RefCell::default(),
            }
        }

        /// Returns the most recent value of an option passed to
        /// [`Self::get_records_with_options()`].
        fn last_get_records_option(&self, option: &str) -> Option<String> {
            self.last_get_records_options
                .borrow()
                .as_ref()
                .and_then(|options| {
                    options
                        .iter_query_pairs()
                        .find(|(key, _)| key == option)
                        .map(|(_, value)| value.into())
                })
        }
    }

    impl SuggestRemoteSettingsClient for SnapshotSettingsClient {
        fn get_records_with_options(
            &self,
            options: &GetItemsOptions,
        ) -> Result<RemoteSettingsResponse> {
            *self.last_get_records_options.borrow_mut() = Some(options.clone());
            let records = self.snapshot.borrow().records.clone();
            let last_modified = records
                .iter()
                .map(|record| record.last_modified)
                .max()
                .unwrap_or(0);
            Ok(RemoteSettingsResponse {
                records,
                last_modified,
            })
        }

        fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
            Ok(self
                .snapshot
                .borrow()
                .attachments
                .get(location)
                .unwrap_or_else(|| unreachable!("Unexpected request for attachment `{}`", location))
                .clone())
        }
    }

    fn before_each() {
        static ONCE: Once = Once::new();
        ONCE.call_once(|| {
            env_logger::init();
        });
    }

    /// Tests that `SuggestStore` is usable with UniFFI, which requires exposed
    /// interfaces to be `Send` and `Sync`.
    #[test]
    fn is_thread_safe() {
        before_each();

        fn is_send_sync<T: Send + Sync>() {}
        is_send_sync::<SuggestStore>();
    }

    /// Tests ingesting suggestions into an empty database.
    #[test]
    fn ingest_suggestions() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "1234",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15)
            );
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting suggestions with icons.
    #[test]
    fn ingest_icons() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url"
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                105,
                                45,
                                97,
                                109,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.2,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
                        raw_url: "https://penne.biz",
                        icon: Some(
                            [
                                105,
                                45,
                                97,
                                109,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "penne",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "pe".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn ingest_full_keywords() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "2",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "3",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-3.json",
                "mimetype": "application/json",
                "location": "data-3.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "4",
            "type": "amp-mobile-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-4.json",
                "mimetype": "application/json",
                "location": "data-4.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        // AMP attachment with full keyword data
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "full_keywords": [
                    // Full keyword for the first 4 keywords
                    ("los pollos", 4),
                    // Full keyword for the next 2 keywords
                    ("los pollos hermanos (restaurant)", 2),
                ],
                "title": "Los Pollos Hermanos - Albuquerque - 1",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?
        // AMP attachment without a full keyword
        .with_data(
            "data-2.json",
            json!([{
                "id": 1,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque - 2",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?
        // Wikipedia attachment with full keyword data.  We should ignore the full
        // keyword data for Wikipedia suggestions
        .with_data(
            "data-3.json",
            json!([{
                "id": 2,
                "advertiser": "Wikipedia",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque - Wiki",
                "full_keywords": [
                    ("Los Pollos Hermanos - Albuquerque", 6),
                ],
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "score": 0.3,
            }]),
        )?
        // Amp mobile suggestion, this is essentially the same as 1, except for the SuggestionProvider
        .with_data(
            "data-4.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "full_keywords": [
                    // Full keyword for the first 4 keywords
                    ("los pollos", 4),
                    // Full keyword for the next 2 keywords
                    ("los pollos hermanos (restaurant)", 2),
                ],
                "title": "Los Pollos Hermanos - Albuquerque - 4",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            // This one should match the first full keyword for the first AMP item.
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque - 1",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los pollos",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque - 2",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los",
                        block_id: 1,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            // This one should match the second full keyword for the first AMP item.
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque - 1",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los pollos hermanos (restaurant)",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque - 2",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los pollos hermanos",
                        block_id: 1,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "los pollos h".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            // This one matches a Wikipedia suggestion, so the full keyword should be ignored
            expect![[r#"
                [
                    Wikipedia {
                        title: "Los Pollos Hermanos - Albuquerque - Wiki",
                        url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los",
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "los".into(),
                providers: vec![SuggestionProvider::Wikipedia],
                limit: None,
            })?);
            // This one matches a Wikipedia suggestion, so the full keyword should be ignored
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque - 4",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los pollos hermanos (restaurant)",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "los pollos h".into(),
                providers: vec![SuggestionProvider::AmpMobile],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting a data attachment containing a single suggestion,
    /// instead of an array of suggestions.
    #[test]
    fn ingest_one_suggestion_in_data_attachment() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!({
                "id": 0,
                 "advertiser": "Good Place Eats",
                 "iab_category": "8 - Food & Drink",
                 "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                 "title": "Lasagna Come Out Tomorrow",
                 "url": "https://www.lasagna.restaurant",
                 "icon": "2",
                 "impression_url": "https://example.com/impression_url",
                 "click_url": "https://example.com/click_url",
                 "score": 0.3
            }),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting suggestions from an updated attachment.
    #[test]
    fn reingest_amp_suggestions() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "1",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }, {
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los p", "los pollos h"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15u64)
            );
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        // Update the snapshot with new suggestions: drop Lasagna, update Los
        // Pollos, and add Penne.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 30,
            "attachment": {
                "filename": "data-1-1.json",
                "mimetype": "application/json",
                "location": "data-1-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["los ", "los pollos", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Now Serving at 14 Locations!",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao: &SuggestDao<'_>| {
            assert_eq!(
                dao.get_meta(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(30u64)
            );
            assert!(dao
                .fetch_suggestions(&SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![SuggestionProvider::Amp],
                    limit: None,
                })?
                .is_empty());
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Now Serving at 14 Locations!",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los pollos",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "los ".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Penne for Your Thoughts",
                        url: "https://penne.biz",
                        raw_url: "https://penne.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "penne",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "pe".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting icons from an updated attachment.
    #[test]
    fn reingest_icons() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions and icons from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }, {
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los pollos", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "3",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?
        .with_icon("icon-2.png", "lasagna-icon".as_bytes().into())
        .with_icon("icon-3.png", "pollos-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(25u64)
            );
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                2
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 2);
            Ok(())
        })?;

        // Update the snapshot with new icons.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "icon-2",
            "type": "icon",
            "last_modified": 30,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 35,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            }
        }]))?
        .with_icon("icon-2.png", "new-lasagna-icon".as_bytes().into())
        .with_icon("icon-3.png", "new-pollos-icon".as_bytes().into());

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(35u64)
            );
            expect![[r#"
                [
                    Amp {
                        title: "Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                110,
                                101,
                                119,
                                45,
                                108,
                                97,
                                115,
                                97,
                                103,
                                110,
                                97,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "la".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: Some(
                            [
                                110,
                                101,
                                119,
                                45,
                                112,
                                111,
                                108,
                                108,
                                111,
                                115,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "los",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests re-ingesting AMO suggestions from an updated attachment.
    #[test]
    fn reingest_amo_suggestions() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "amo-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-2",
            "type": "amo-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!({
                "description": "First suggestion",
                "url": "https://example.org/amo-suggestion-1",
                "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                "keywords": ["relay", "spam", "masking email", "alias"],
                "title": "AMO suggestion",
                "icon": "https://example.org/amo-suggestion-1/icon.png",
                "rating": "4.9",
                "number_of_ratings": 800,
                "score": 0.25
            }),
        )?
        .with_data(
            "data-2.json",
            json!([{
                "description": "Second suggestion",
                "url": "https://example.org/amo-suggestion-2",
                "guid": "{6d24e3b8-1400-4d37-9440-c798f9b79b1a}",
                "keywords": ["dark mode", "dark theme", "night mode"],
                "title": "Another AMO suggestion",
                "icon": "https://example.org/amo-suggestion-2/icon.png",
                "rating": "4.6",
                "number_of_ratings": 750,
                "score": 0.25
            }, {
                "description": "Third suggestion",
                "url": "https://example.org/amo-suggestion-3",
                "guid": "{1e9d493b-0498-48bb-9b9a-8b45a44df146}",
                "keywords": ["grammar", "spelling", "edit"],
                "title": "Yet another AMO suggestion",
                "icon": "https://example.org/amo-suggestion-3/icon.png",
                "rating": "4.8",
                "number_of_ratings": 900,
                "score": 0.25
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(SuggestRecordType::Amo.last_ingest_meta_key().as_str())?,
                Some(15u64)
            );

            expect![[r#"
                [
                    Amo {
                        title: "AMO suggestion",
                        url: "https://example.org/amo-suggestion-1",
                        icon_url: "https://example.org/amo-suggestion-1/icon.png",
                        description: "First suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 800,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "masking e".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            expect![[r#"
                [
                    Amo {
                        title: "Another AMO suggestion",
                        url: "https://example.org/amo-suggestion-2",
                        icon_url: "https://example.org/amo-suggestion-2/icon.png",
                        description: "Second suggestion",
                        rating: Some(
                            "4.6",
                        ),
                        number_of_ratings: 750,
                        guid: "{6d24e3b8-1400-4d37-9440-c798f9b79b1a}",
                        score: 0.25,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "night".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            Ok(())
        })?;

        // Update the snapshot with new suggestions: update the second, drop the
        // third, and add the fourth.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "data-2",
            "type": "amo-suggestions",
            "last_modified": 30,
            "attachment": {
                "filename": "data-2-1.json",
                "mimetype": "application/json",
                "location": "data-2-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-2-1.json",
            json!([{
                "description": "Updated second suggestion",
                "url": "https://example.org/amo-suggestion-2",
                "guid": "{6d24e3b8-1400-4d37-9440-c798f9b79b1a}",
                "keywords": ["dark mode", "night mode"],
                "title": "Another AMO suggestion",
                "icon": "https://example.org/amo-suggestion-2/icon.png",
                "rating": "4.7",
                "number_of_ratings": 775,
                "score": 0.25
            }, {
                "description": "Fourth suggestion",
                "url": "https://example.org/amo-suggestion-4",
                "guid": "{1ea82ebd-a1ba-4f57-b8bb-3824ead837bd}",
                "keywords": ["image search", "visual search"],
                "title": "New AMO suggestion",
                "icon": "https://example.org/amo-suggestion-4/icon.png",
                "rating": "5.0",
                "number_of_ratings": 100,
                "score": 0.25
            }]),
        )?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(SuggestRecordType::Amo.last_ingest_meta_key().as_str())?,
                Some(30u64)
            );

            expect![[r#"
                [
                    Amo {
                        title: "AMO suggestion",
                        url: "https://example.org/amo-suggestion-1",
                        icon_url: "https://example.org/amo-suggestion-1/icon.png",
                        description: "First suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 800,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "masking e".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            expect![[r#"
                []
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "dark t".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            expect![[r#"
                [
                    Amo {
                        title: "Another AMO suggestion",
                        url: "https://example.org/amo-suggestion-2",
                        icon_url: "https://example.org/amo-suggestion-2/icon.png",
                        description: "Updated second suggestion",
                        rating: Some(
                            "4.7",
                        ),
                        number_of_ratings: 775,
                        guid: "{6d24e3b8-1400-4d37-9440-c798f9b79b1a}",
                        score: 0.25,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "night".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            expect![[r#"
                [
                    Amo {
                        title: "New AMO suggestion",
                        url: "https://example.org/amo-suggestion-4",
                        icon_url: "https://example.org/amo-suggestion-4/icon.png",
                        description: "Fourth suggestion",
                        rating: Some(
                            "5.0",
                        ),
                        number_of_ratings: 100,
                        guid: "{1ea82ebd-a1ba-4f57-b8bb-3824ead837bd}",
                        score: 0.25,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "image search".into(),
                providers: vec![SuggestionProvider::Amo],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting tombstones for previously-ingested suggestions and
    /// icons.
    #[test]
    fn ingest_tombstones() -> anyhow::Result<()> {
        before_each();

        // Ingest suggestions and icons from the initial snapshot.
        let initial_snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(initial_snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                1
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 1);
            assert_eq!(
                dao.get_meta(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15)
            );
            assert_eq!(
                dao.get_meta(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(20)
            );

            Ok(())
        })?;

        // Replace the records with tombstones. Ingesting these should remove
        // all their suggestions and icons.
        *store.settings_client.snapshot.borrow_mut() = Snapshot::with_records(json!([{
            "id": "data-1",
            "last_modified": 25,
            "deleted": true,
        }, {
            "id": "icon-2",
            "last_modified": 30,
            "deleted": true,
        }]))?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 0);
            assert_eq!(
                dao.get_meta(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(30)
            );
            Ok(())
        })?;

        Ok(())
    }

    /// Tests ingesting suggestions with constraints.
    #[test]
    fn ingest_with_constraints() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;
        assert_eq!(
            store.settings_client.last_get_records_option("_limit"),
            None,
        );

        // 200 suggestions per record, so test with numbers around that
        // boundary.
        let table = [
            (0, "1"),
            (199, "1"),
            (200, "1"),
            (201, "2"),
            (300, "2"),
            (400, "2"),
            (401, "3"),
        ];
        for (max_suggestions, expected_limit) in table {
            store.ingest(SuggestIngestionConstraints {
                max_suggestions: Some(max_suggestions),
                providers: Some(vec![SuggestionProvider::Amp]),
            })?;
            let actual_limit = store
                .settings_client
                .last_get_records_option("_limit")
                .ok_or_else(|| {
                    anyhow!("Want limit = {} for {}", expected_limit, max_suggestions)
                })?;
            assert_eq!(
                actual_limit, expected_limit,
                "Want limit = {} for {}; got limit = {}",
                expected_limit, max_suggestions, actual_limit
            );
        }

        Ok(())
    }

    /// Tests clearing the store.
    #[test]
    fn clear() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "2",
                "impression_url": "https://example.com",
                "click_url": "https://example.com",
                "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15)
            );
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                1
            );
            assert_eq!(
                dao.conn.query_one::<i64>("SELECT count(*) FROM keywords")?,
                6
            );

            Ok(())
        })?;

        store.clear()?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                None
            );
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(
                dao.conn.query_one::<i64>("SELECT count(*) FROM keywords")?,
                0
            );

            Ok(())
        })?;

        Ok(())
    }

    /// Tests querying suggestions.
    #[test]
    fn query() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },

        }, {
            "id": "data-2",
            "type": "amo-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-3",
            "type": "pocket-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-3.json",
                "mimetype": "application/json",
                "location": "data-3.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-4",
            "type": "yelp-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-4.json",
                "mimetype": "application/json",
                "location": "data-4.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-5",
            "type": "mdn-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-5.json",
                "mimetype": "application/json",
                "location": "data-5.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-yelp-favicon",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "yelp-favicon.svg",
                "mimetype": "image/svg+xml",
                "location": "yelp-favicon.svg",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cal", "cali", "california"],
                "title": "California",
                "url": "https://wikipedia.org/California",
                "icon": "3"
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cal", "cali", "california", "institute", "technology"],
                "title": "California Institute of Technology",
                "url": "https://wikipedia.org/California_Institute_of_Technology",
                "icon": "3"
            },{
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["multimatch"],
                "title": "Multimatch",
                "url": "https://wikipedia.org/Multimatch",
                "icon": "3"
            }]),
        )?
            .with_data(
                "data-2.json",
                json!([
                    {
                        "description": "amo suggestion",
                        "url": "https://addons.mozilla.org/en-US/firefox/addon/example",
                        "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        "keywords": ["relay", "spam", "masking email", "alias"],
                        "title": "Firefox Relay",
                        "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        "rating": "4.9",
                        "number_of_ratings": 888,
                        "score": 0.25
                    },
                    {
                        "description": "amo suggestion multi-match",
                        "url": "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                        "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        "keywords": ["multimatch"],
                        "title": "Firefox Multimatch",
                        "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        "rating": "4.9",
                        "number_of_ratings": 888,
                        "score": 0.25
                    },
                ]),
        )?
            .with_data(
            "data-3.json",
            json!([
                {
                    "description": "pocket suggestion",
                    "url": "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                    "lowConfidenceKeywords": ["soft life", "workaholism", "toxic work culture", "work-life balance"],
                    "highConfidenceKeywords": ["burnout women", "grind culture", "women burnout"],
                    "title": "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                    "score": 0.25
                },
                {
                    "description": "pocket suggestion multi-match",
                    "url": "https://getpocket.com/collections/multimatch",
                    "lowConfidenceKeywords": [],
                    "highConfidenceKeywords": ["multimatch"],
                    "title": "Multimatching",
                    "score": 0.88
                },
            ]),
        )?
        .with_data(
            "data-4.json",
            json!({
                "subjects": ["ramen", "spicy ramen", "spicy random ramen", "rats", "raven", "raccoon", "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789", "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789Z"],
                "preModifiers": ["best", "super best", "same_modifier"],
                "postModifiers": ["delivery", "super delivery", "same_modifier"],
                "locationSigns": [
                    { "keyword": "in", "needLocation": true },
                    { "keyword": "near", "needLocation": true },
                    { "keyword": "near by", "needLocation": false },
                    { "keyword": "near me", "needLocation": false },
                ],
                "yelpModifiers": ["yelp", "yelp keyword"],
                "icon": "yelp-favicon",
                "score": 0.5
            }),
        )?
        .with_data(
            "data-5.json",
            json!([
                {
                    "description": "Javascript Array",
                    "url": "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                    "keywords": ["array javascript", "javascript array", "wildcard"],
                    "title": "Array",
                    "score": 0.24
                },
            ]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into())
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into())
        .with_icon("yelp-favicon.svg", "yelp-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "empty keyword; all providers",
                SuggestionQuery {
                    keyword: String::new(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                        SuggestionProvider::Yelp,
                        SuggestionProvider::Weather,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; all providers",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                        SuggestionProvider::Yelp,
                        SuggestionProvider::Weather,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: Some(
                                [
                                    105,
                                    45,
                                    97,
                                    109,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "lasagna",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                            score: 0.3,
                        },
                    ]
                "#]],
            ),
            (
                "multimatch; all providers",
                SuggestionQuery {
                    keyword: "multimatch".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "Multimatching",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.88,
                            is_top_pick: true,
                        },
                        Amo {
                            title: "Firefox Multimatch",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion multi-match",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "multimatch",
                        },
                    ]
                "#]],
            ),
            (
                "MultiMatch; all providers, mixed case",
                SuggestionQuery {
                    keyword: "MultiMatch".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "Multimatching",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.88,
                            is_top_pick: true,
                        },
                        Amo {
                            title: "Firefox Multimatch",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion multi-match",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "multimatch",
                        },
                    ]
                "#]],
            ),
            (
                "multimatch; all providers, limit 2",
                SuggestionQuery {
                    keyword: "multimatch".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: Some(2),
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "Multimatching",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.88,
                            is_top_pick: true,
                        },
                        Amo {
                            title: "Firefox Multimatch",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/multimatch",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion multi-match",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; AMP only",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![SuggestionProvider::Amp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: Some(
                                [
                                    105,
                                    45,
                                    97,
                                    109,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "lasagna",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                            score: 0.3,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `la`; Wikipedia, AMO, and Pocket",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `la`; no providers",
                SuggestionQuery {
                    keyword: "la".into(),
                    providers: vec![],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; AMP, AMO, and Pocket",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `cal`; Wikipedia only",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![SuggestionProvider::Wikipedia],
                    limit: None,
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "California",
                            url: "https://wikipedia.org/California",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "california",
                        },
                        Wikipedia {
                            title: "California Institute of Technology",
                            url: "https://wikipedia.org/California_Institute_of_Technology",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "california",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `cal`; Wikipedia with limit 1",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![SuggestionProvider::Wikipedia],
                    limit: Some(1),
                },
                expect![[r#"
                    [
                        Wikipedia {
                            title: "California",
                            url: "https://wikipedia.org/California",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "california",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `cal`; no providers",
                SuggestionQuery {
                    keyword: "cal".into(),
                    providers: vec![],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `spam`; AMO only",
                SuggestionQuery {
                    keyword: "spam".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking`; AMO only",
                SuggestionQuery {
                    keyword: "masking".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking e`; AMO only",
                SuggestionQuery {
                    keyword: "masking e".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                [
                    Amo {
                        title: "Firefox Relay",
                        url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                        icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        description: "amo suggestion",
                        rating: Some(
                            "4.9",
                        ),
                        number_of_ratings: 888,
                        guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        score: 0.25,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `masking s`; AMO only",
                SuggestionQuery {
                    keyword: "masking s".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `soft`; AMP and Wikipedia",
                SuggestionQuery {
                    keyword: "soft".into(),
                    providers: vec![SuggestionProvider::Amp, SuggestionProvider::Wikipedia],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `soft`; Pocket only",
                SuggestionQuery {
                    keyword: "soft".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: false,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `soft l`; Pocket only",
                SuggestionQuery {
                    keyword: "soft l".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: false,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `sof`; Pocket only",
                SuggestionQuery {
                    keyword: "sof".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = `burnout women`; Pocket only",
                SuggestionQuery {
                    keyword: "burnout women".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                [
                    Pocket {
                        title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                        url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                        score: 0.25,
                        is_top_pick: true,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `burnout person`; Pocket only",
                SuggestionQuery {
                    keyword: "burnout person".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `best spicy ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "best spicy ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=best+spicy+ramen+delivery&find_loc=tokyo",
                            title: "best spicy ramen delivery in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `BeSt SpIcY rAmEn DeLiVeRy In ToKyO`; Yelp only",
                SuggestionQuery {
                    keyword: "BeSt SpIcY rAmEn DeLiVeRy In ToKyO".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=BeSt+SpIcY+rAmEn+DeLiVeRy&find_loc=ToKyO",
                            title: "BeSt SpIcY rAmEn DeLiVeRy In ToKyO",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `best ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "best ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=best+ramen+delivery&find_loc=tokyo",
                            title: "best ramen delivery in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `best invalid_ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "best invalid_ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `best delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "best delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `super best ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "super best ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=super+best+ramen+delivery&find_loc=tokyo",
                            title: "super best ramen delivery in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `invalid_best ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "invalid_best ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `ramen delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen+delivery&find_loc=tokyo",
                            title: "ramen delivery in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen super delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen super delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen+super+delivery&find_loc=tokyo",
                            title: "ramen super delivery in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen invalid_delivery in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen invalid_delivery in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `ramen in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
                            title: "ramen in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen near tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen near tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
                            title: "ramen near tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen invalid_in tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen invalid_in tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `ramen in San Francisco`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen in San Francisco".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen&find_loc=San+Francisco",
                            title: "ramen in San Francisco",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen in`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen in".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen",
                            title: "ramen in",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen near by`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen near by".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen+near+by",
                            title: "ramen near by",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen near me`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen near me".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen+near+me",
                            title: "ramen near me",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen near by tokyo`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen near by tokyo".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `ramen`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen",
                            title: "ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = maximum chars; Yelp only",
                SuggestionQuery {
                    keyword: "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789",
                            title: "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = over chars; Yelp only",
                SuggestionQuery {
                    keyword: "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789Z".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `best delivery`; Yelp only",
                SuggestionQuery {
                    keyword: "best delivery".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `same_modifier same_modifier`; Yelp only",
                SuggestionQuery {
                    keyword: "same_modifier same_modifier".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `same_modifier `; Yelp only",
                SuggestionQuery {
                    keyword: "same_modifier ".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `yelp ramen`; Yelp only",
                SuggestionQuery {
                    keyword: "yelp ramen".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen",
                            title: "ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `yelp keyword ramen`; Yelp only",
                SuggestionQuery {
                    keyword: "yelp keyword ramen".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen",
                            title: "ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen in tokyo yelp`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen in tokyo yelp".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
                            title: "ramen in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `ramen in tokyo yelp keyword`; Yelp only",
                SuggestionQuery {
                    keyword: "ramen in tokyo yelp keyword".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
                            title: "ramen in tokyo",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: true,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `yelp ramen yelp`; Yelp only",
                SuggestionQuery {
                    keyword: "yelp ramen yelp".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=ramen",
                            title: "ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `best yelp ramen`; Yelp only",
                SuggestionQuery {
                    keyword: "best yelp ramen".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `Spicy R`; Yelp only",
                SuggestionQuery {
                    keyword: "Spicy R".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=Spicy+Ramen",
                            title: "Spicy Ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: false,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `BeSt             Ramen`; Yelp only",
                SuggestionQuery {
                    keyword: "BeSt             Ramen".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=BeSt+Ramen",
                            title: "BeSt Ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: true,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `BeSt             Spicy R`; Yelp only",
                SuggestionQuery {
                    keyword: "BeSt             Spicy R".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                    [
                        Yelp {
                            url: "https://www.yelp.com/search?find_desc=BeSt+Spicy+Ramen",
                            title: "BeSt Spicy Ramen",
                            icon: Some(
                                [
                                    121,
                                    101,
                                    108,
                                    112,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/svg+xml",
                            ),
                            score: 0.5,
                            has_location_sign: false,
                            subject_exact_match: false,
                            location_param: "find_loc",
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `BeSt             R`; Yelp only",
                SuggestionQuery {
                    keyword: "BeSt             R".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `r`; Yelp only",
                SuggestionQuery {
                    keyword: "r".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `ra`; Yelp only",
                SuggestionQuery {
                    keyword: "ra".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                [
                    Yelp {
                        url: "https://www.yelp.com/search?find_desc=rats",
                        title: "rats",
                        icon: Some(
                            [
                                121,
                                101,
                                108,
                                112,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/svg+xml",
                        ),
                        score: 0.5,
                        has_location_sign: false,
                        subject_exact_match: false,
                        location_param: "find_loc",
                    },
                ]
                "#]],
            ),
            (
                "keyword = `ram`; Yelp only",
                SuggestionQuery {
                    keyword: "ram".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                [
                    Yelp {
                        url: "https://www.yelp.com/search?find_desc=ramen",
                        title: "ramen",
                        icon: Some(
                            [
                                121,
                                101,
                                108,
                                112,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/svg+xml",
                        ),
                        score: 0.5,
                        has_location_sign: false,
                        subject_exact_match: false,
                        location_param: "find_loc",
                    },
                ]
                "#]],
            ),
            (
                "keyword = `rac`; Yelp only",
                SuggestionQuery {
                    keyword: "rac".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                [
                    Yelp {
                        url: "https://www.yelp.com/search?find_desc=raccoon",
                        title: "raccoon",
                        icon: Some(
                            [
                                121,
                                101,
                                108,
                                112,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/svg+xml",
                        ),
                        score: 0.5,
                        has_location_sign: false,
                        subject_exact_match: false,
                        location_param: "find_loc",
                    },
                ]
                "#]],
            ),
            (
                "keyword = `best r`; Yelp only",
                SuggestionQuery {
                    keyword: "best r".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                []
                "#]],
            ),
            (
                "keyword = `best ra`; Yelp only",
                SuggestionQuery {
                    keyword: "best ra".into(),
                    providers: vec![SuggestionProvider::Yelp],
                    limit: None,
                },
                expect![[r#"
                [
                    Yelp {
                        url: "https://www.yelp.com/search?find_desc=best+rats",
                        title: "best rats",
                        icon: Some(
                            [
                                121,
                                101,
                                108,
                                112,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/svg+xml",
                        ),
                        score: 0.5,
                        has_location_sign: false,
                        subject_exact_match: false,
                        location_param: "find_loc",
                    },
                ]
                "#]],
            ),
        ];
        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    // Tests querying amp wikipedia
    #[test]
    fn query_with_multiple_providers_and_diff_scores() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-2",
            "type": "pocket-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow", "amp wiki match"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }, {
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["pe", "pen", "penne", "penne for your thoughts", "amp wiki match"],
                "title": "Penne for Your Thoughts",
                "url": "https://penne.biz",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.1
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["amp wiki match", "pocket wiki match"],
                "title": "Multimatch",
                "url": "https://wikipedia.org/Multimatch",
                "icon": "3"
            }]),
        )?
        .with_data(
            "data-2.json",
            json!([
                {
                    "description": "pocket suggestion",
                    "url": "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                    "lowConfidenceKeywords": ["soft life", "workaholism", "toxic work culture", "work-life balance", "pocket wiki match"],
                    "highConfidenceKeywords": ["burnout women", "grind culture", "women burnout"],
                    "title": "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                    "score": 0.05
                },
                {
                    "description": "pocket suggestion multi-match",
                    "url": "https://getpocket.com/collections/multimatch",
                    "lowConfidenceKeywords": [],
                    "highConfidenceKeywords": ["pocket wiki match"],
                    "title": "Pocket wiki match",
                    "score": 0.88
                },
            ]),
        )?
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "keyword = `amp wiki match`; all providers",
                SuggestionQuery {
                    keyword: "amp wiki match".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                        SuggestionProvider::Yelp,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: None,
                            icon_mimetype: None,
                            full_keyword: "amp wiki match",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                            score: 0.3,
                        },
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "amp wiki match",
                        },
                        Amp {
                            title: "Penne for Your Thoughts",
                            url: "https://penne.biz",
                            raw_url: "https://penne.biz",
                            icon: None,
                            icon_mimetype: None,
                            full_keyword: "amp wiki match",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                            score: 0.1,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `amp wiki match`; all providers, limit 2",
                SuggestionQuery {
                    keyword: "amp wiki match".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                        SuggestionProvider::Yelp,
                    ],
                    limit: Some(2),
                },
                expect![[r#"
                    [
                        Amp {
                            title: "Lasagna Come Out Tomorrow",
                            url: "https://www.lasagna.restaurant",
                            raw_url: "https://www.lasagna.restaurant",
                            icon: None,
                            icon_mimetype: None,
                            full_keyword: "amp wiki match",
                            block_id: 0,
                            advertiser: "Good Place Eats",
                            iab_category: "8 - Food & Drink",
                            impression_url: "https://example.com/impression_url",
                            click_url: "https://example.com/click_url",
                            raw_click_url: "https://example.com/click_url",
                            score: 0.3,
                        },
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "amp wiki match",
                        },
                    ]
                "#]],
            ),
            (
                "pocket wiki match; all providers",
                SuggestionQuery {
                    keyword: "pocket wiki match".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: None,
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "Pocket wiki match",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.88,
                            is_top_pick: true,
                        },
                        Wikipedia {
                            title: "Multimatch",
                            url: "https://wikipedia.org/Multimatch",
                            icon: Some(
                                [
                                    97,
                                    108,
                                    115,
                                    111,
                                    45,
                                    97,
                                    110,
                                    45,
                                    105,
                                    99,
                                    111,
                                    110,
                                ],
                            ),
                            icon_mimetype: Some(
                                "image/png",
                            ),
                            full_keyword: "pocket wiki match",
                        },
                        Pocket {
                            title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                            url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                            score: 0.05,
                            is_top_pick: false,
                        },
                    ]
                "#]],
            ),
            (
                "pocket wiki match; all providers limit 1",
                SuggestionQuery {
                    keyword: "pocket wiki match".into(),
                    providers: vec![
                        SuggestionProvider::Amp,
                        SuggestionProvider::Wikipedia,
                        SuggestionProvider::Amo,
                        SuggestionProvider::Pocket,
                    ],
                    limit: Some(1),
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "Pocket wiki match",
                            url: "https://getpocket.com/collections/multimatch",
                            score: 0.88,
                            is_top_pick: true,
                        },
                    ]
                "#]],
            ),
            (
                "work-life balance; duplicate providers",
                SuggestionQuery {
                    keyword: "work-life balance".into(),
                    providers: vec![SuggestionProvider::Pocket, SuggestionProvider::Pocket],
                    limit: Some(-1),
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                            url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                            score: 0.05,
                            is_top_pick: false,
                        },
                    ]
                "#]],
            ),
        ];
        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    // Tests querying multiple suggestions with multiple keywords with same prefix keyword
    #[test]
    fn query_with_multiple_suggestions_with_same_prefix() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
             "id": "data-1",
             "type": "amo-suggestions",
             "last_modified": 15,
             "attachment": {
                 "filename": "data-1.json",
                 "mimetype": "application/json",
                 "location": "data-1.json",
                 "hash": "",
                 "size": 0,
             },
         }, {
             "id": "data-2",
             "type": "pocket-suggestions",
             "last_modified": 15,
             "attachment": {
                 "filename": "data-2.json",
                 "mimetype": "application/json",
                 "location": "data-2.json",
                 "hash": "",
                 "size": 0,
             },
         }, {
             "id": "icon-3",
             "type": "icon",
             "last_modified": 25,
             "attachment": {
                 "filename": "icon-3.png",
                 "mimetype": "image/png",
                 "location": "icon-3.png",
                 "hash": "",
                 "size": 0,
             },
         }]))?
         .with_data(
             "data-1.json",
             json!([
                    {
                    "description": "amo suggestion",
                    "url": "https://addons.mozilla.org/en-US/firefox/addon/example",
                    "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                    "keywords": ["relay", "spam", "masking email", "masking emails", "masking accounts", "alias" ],
                    "title": "Firefox Relay",
                    "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                    "rating": "4.9",
                    "number_of_ratings": 888,
                    "score": 0.25
                }
            ]),
         )?
         .with_data(
             "data-2.json",
             json!([
                 {
                     "description": "pocket suggestion",
                     "url": "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                     "lowConfidenceKeywords": ["soft life", "soft living", "soft work", "workaholism", "toxic work culture"],
                     "highConfidenceKeywords": ["burnout women", "grind culture", "women burnout", "soft lives"],
                     "title": "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                     "score": 0.05
                 }
             ]),
         )?
         .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "keyword = `soft li`; pocket",
                SuggestionQuery {
                    keyword: "soft li".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                            url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                            score: 0.05,
                            is_top_pick: false,
                        },
                    ]
                 "#]],
            ),
            (
                "keyword = `soft lives`; pocket",
                SuggestionQuery {
                    keyword: "soft lives".into(),
                    providers: vec![SuggestionProvider::Pocket],
                    limit: None,
                },
                expect![[r#"
                    [
                        Pocket {
                            title: "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                            url: "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                            score: 0.05,
                            is_top_pick: true,
                        },
                    ]
                 "#]],
            ),
            (
                "keyword = `masking `; amo provider",
                SuggestionQuery {
                    keyword: "masking ".into(),
                    providers: vec![SuggestionProvider::Amo],
                    limit: None,
                },
                expect![[r#"
                    [
                        Amo {
                            title: "Firefox Relay",
                            url: "https://addons.mozilla.org/en-US/firefox/addon/example",
                            icon_url: "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                            description: "amo suggestion",
                            rating: Some(
                                "4.9",
                            ),
                            number_of_ratings: 888,
                            guid: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                            score: 0.25,
                        },
                    ]
                 "#]],
            ),
        ];
        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    // Tests querying multiple suggestions with multiple keywords with same prefix keyword
    #[test]
    fn query_with_amp_mobile_provider() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "amp-mobile-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-2",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([
               {
                   "id": 0,
                   "advertiser": "Good Place Eats",
                   "iab_category": "8 - Food & Drink",
                   "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                   "title": "Mobile - Lasagna Come Out Tomorrow",
                   "url": "https://www.lasagna.restaurant",
                   "icon": "3",
                   "impression_url": "https://example.com/impression_url",
                   "click_url": "https://example.com/click_url",
                   "score": 0.3
               }
            ]),
        )?
        .with_data(
            "data-2.json",
            json!([
              {
                  "id": 0,
                  "advertiser": "Good Place Eats",
                  "iab_category": "8 - Food & Drink",
                  "keywords": ["la", "las", "lasa", "lasagna", "lasagna come out tomorrow"],
                  "title": "Desktop - Lasagna Come Out Tomorrow",
                  "url": "https://www.lasagna.restaurant",
                  "icon": "3",
                  "impression_url": "https://example.com/impression_url",
                  "click_url": "https://example.com/click_url",
                  "score": 0.2
              }
            ]),
        )?
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "keyword = `las`; Amp Mobile",
                SuggestionQuery {
                    keyword: "las".into(),
                    providers: vec![SuggestionProvider::AmpMobile],
                    limit: None,
                },
                expect![[r#"
                [
                    Amp {
                        title: "Mobile - Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                97,
                                108,
                                115,
                                111,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `las`; Amp",
                SuggestionQuery {
                    keyword: "las".into(),
                    providers: vec![SuggestionProvider::Amp],
                    limit: None,
                },
                expect![[r#"
                [
                    Amp {
                        title: "Desktop - Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                97,
                                108,
                                115,
                                111,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.2,
                    },
                ]
                "#]],
            ),
            (
                "keyword = `las `; amp and amp mobile",
                SuggestionQuery {
                    keyword: "las".into(),
                    providers: vec![SuggestionProvider::Amp, SuggestionProvider::AmpMobile],
                    limit: None,
                },
                expect![[r#"
                [
                    Amp {
                        title: "Mobile - Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                97,
                                108,
                                115,
                                111,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                    Amp {
                        title: "Desktop - Lasagna Come Out Tomorrow",
                        url: "https://www.lasagna.restaurant",
                        raw_url: "https://www.lasagna.restaurant",
                        icon: Some(
                            [
                                97,
                                108,
                                115,
                                111,
                                45,
                                97,
                                110,
                                45,
                                105,
                                99,
                                111,
                                110,
                            ],
                        ),
                        icon_mimetype: Some(
                            "image/png",
                        ),
                        full_keyword: "lasagna",
                        block_id: 0,
                        advertiser: "Good Place Eats",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.2,
                    },
                ]
                "#]],
            ),
        ];
        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    /// Tests ingesting malformed Remote Settings records that we understand,
    /// but that are missing fields, or aren't in the format we expect.
    #[test]
    fn ingest_malformed() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            // Data record without an attachment.
            "id": "missing-data-attachment",
            "type": "data",
            "last_modified": 15,
        }, {
            // Icon record without an attachment.
            "id": "missing-icon-attachment",
            "type": "icon",
            "last_modified": 30,
        }, {
            // Icon record with an ID that's not `icon-{id}`, so suggestions in
            // the data attachment won't be able to reference it.
            "id": "bad-icon-id",
            "type": "icon",
            "last_modified": 45,
            "attachment": {
                "filename": "icon-1.png",
                "mimetype": "image/png",
                "location": "icon-1.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_icon("icon-1.png", "i-am-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(45)
            );
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 0);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests unparsable Remote Settings records, which we don't know how to
    /// ingest at all.
    #[test]
    fn ingest_unparsable() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        }, {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 30,
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta("last_quicksuggest_ingest_unparsable")?,
                Some(30)
            );
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 19,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 19,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn ingest_mixed_parsable_unparsable_records() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        },
        {
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        },
        {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 30,
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3,
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta("last_quicksuggest_ingest_unparsable")?,
                Some(30)
            );
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 19,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 19,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests meta update field isn't updated for old unparsable Remote Settings
    /// records.
    #[test]
    fn ingest_unparsable_and_meta_update_stays_the_same() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.dbs()?.writer.write(|dao| {
            dao.put_meta(
                SuggestRecordType::AmpWikipedia
                    .last_ingest_meta_key()
                    .as_str(),
                30,
            )?;
            Ok(())
        })?;
        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(30)
            );
            Ok(())
        })?;

        Ok(())
    }

    /// Tests that we only ingest providers that we're concerned with.
    #[test]
    fn ingest_constraints_provider() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
        }, {
            "id": "icon-1",
            "type": "icon",
            "last_modified": 30,
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.dbs()?.writer.write(|dao| {
            // Check that existing data is updated properly.
            dao.put_meta(
                SuggestRecordType::AmpWikipedia
                    .last_ingest_meta_key()
                    .as_str(),
                10,
            )?;
            Ok(())
        })?;

        let constraints = SuggestIngestionConstraints {
            max_suggestions: Some(100),
            providers: Some(vec![SuggestionProvider::Amp, SuggestionProvider::Pocket]),
        };
        store.ingest(constraints)?;

        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15)
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(30)
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Pocket.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Amo.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Yelp.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Mdn.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::AmpMobile.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::GlobalConfig
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                None
            );
            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn remove_known_records_out_of_meta_table() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "fancy-new-suggestions-1",
            "type": "fancy-new-suggestions",
            "last_modified": 15,
        },
        {
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        },
        {
            "id": "clippy-2",
            "type": "clippy",
            "last_modified": 15,
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Los Pollos Hermanos",
                "iab_category": "8 - Food & Drink",
                "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                "title": "Los Pollos Hermanos - Albuquerque",
                "url": "https://www.lph-nm.biz",
                "icon": "5678",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        let mut initial_data = UnparsableRecords::default();
        initial_data
            .0
            .insert("data-1".to_string(), UnparsableRecord { schema_version: 1 });
        initial_data.0.insert(
            "clippy-2".to_string(),
            UnparsableRecord { schema_version: 1 },
        );
        store.dbs()?.writer.write(|dao| {
            dao.put_meta(UNPARSABLE_RECORDS_META_KEY, initial_data)?;
            Ok(())
        })?;

        store.ingest(SuggestIngestionConstraints::default())?;

        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "clippy-2": UnparsableRecord {
                                schema_version: 19,
                            },
                            "fancy-new-suggestions-1": UnparsableRecord {
                                schema_version: 19,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        Ok(())
    }

    /// Tests that records with invalid attachments are ignored and marked as unparsable.
    #[test]
    fn skip_over_invalid_records() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([
            {
                "id": "invalid-attachment",
                "type": "data",
                "last_modified": 15,
                "attachment": {
                    "filename": "data-2.json",
                    "mimetype": "application/json",
                    "location": "data-2.json",
                    "hash": "",
                    "size": 0,
                },
            },
            {
                "id": "valid-record",
                "type": "data",
                "last_modified": 15,
                "attachment": {
                    "filename": "data-1.json",
                    "mimetype": "application/json",
                    "location": "data-1.json",
                    "hash": "",
                    "size": 0,
                },
            },
        ]))?
        .with_data(
            "data-1.json",
            json!([{
                    "id": 0,
                    "advertiser": "Los Pollos Hermanos",
                    "iab_category": "8 - Food & Drink",
                    "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                    "title": "Los Pollos Hermanos - Albuquerque",
                    "url": "https://www.lph-nm.biz",
                    "icon": "5678",
                    "impression_url": "https://example.com/impression_url",
                    "click_url": "https://example.com/click_url",
                    "score": 0.3
            }]),
        )?
        // This attachment is missing the `keywords` field and is invalid
        .with_data(
            "data-2.json",
            json!([{
                    "id": 1,
                    "advertiser": "Los Pollos Hermanos",
                    "iab_category": "8 - Food & Drink",
                    "title": "Los Pollos Hermanos - Albuquerque",
                    "url": "https://www.lph-nm.biz",
                    "icon": "5678",
                    "impression_url": "https://example.com/impression_url",
                    "click_url": "https://example.com/click_url",
                    "score": 0.3
            }]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        // Test that the invalid record marked as unparsable
        store.dbs()?.reader.read(|dao| {
            expect![[r#"
                Some(
                    UnparsableRecords(
                        {
                            "invalid-attachment": UnparsableRecord {
                                schema_version: 19,
                            },
                        },
                    ),
                )
            "#]]
            .assert_debug_eq(&dao.get_meta::<UnparsableRecords>(UNPARSABLE_RECORDS_META_KEY)?);
            Ok(())
        })?;

        // Test that the valid record was read
        store.dbs()?.reader.read(|dao| {
            assert_eq!(
                dao.get_meta(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(15)
            );
            expect![[r#"
                [
                    Amp {
                        title: "Los Pollos Hermanos - Albuquerque",
                        url: "https://www.lph-nm.biz",
                        raw_url: "https://www.lph-nm.biz",
                        icon: None,
                        icon_mimetype: None,
                        full_keyword: "los",
                        block_id: 0,
                        advertiser: "Los Pollos Hermanos",
                        iab_category: "8 - Food & Drink",
                        impression_url: "https://example.com/impression_url",
                        click_url: "https://example.com/click_url",
                        raw_click_url: "https://example.com/click_url",
                        score: 0.3,
                    },
                ]
            "#]]
            .assert_debug_eq(&dao.fetch_suggestions(&SuggestionQuery {
                keyword: "lo".into(),
                providers: vec![SuggestionProvider::Amp],
                limit: None,
            })?);

            Ok(())
        })?;

        Ok(())
    }

    #[test]
    fn unparsable_record_serialized_correctly() -> anyhow::Result<()> {
        let unparseable_record = UnparsableRecord { schema_version: 1 };
        assert_eq!(serde_json::to_value(unparseable_record)?, json!({ "v": 1 }),);
        Ok(())
    }

    #[test]
    fn query_mdn() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "mdn-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([
                {
                    "description": "Javascript Array",
                    "url": "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                    "keywords": ["array javascript", "javascript array", "wildcard"],
                    "title": "Array",
                    "score": 0.24
                },
            ]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "keyword = prefix; MDN only",
                SuggestionQuery {
                    keyword: "array".into(),
                    providers: vec![SuggestionProvider::Mdn],
                    limit: None,
                },
                expect![[r#"
                    [
                        Mdn {
                            title: "Array",
                            url: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                            description: "Javascript Array",
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = prefix + partial suffix; MDN only",
                SuggestionQuery {
                    keyword: "array java".into(),
                    providers: vec![SuggestionProvider::Mdn],
                    limit: None,
                },
                expect![[r#"
                    [
                        Mdn {
                            title: "Array",
                            url: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                            description: "Javascript Array",
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = prefix + entire suffix; MDN only",
                SuggestionQuery {
                    keyword: "javascript array".into(),
                    providers: vec![SuggestionProvider::Mdn],
                    limit: None,
                },
                expect![[r#"
                    [
                        Mdn {
                            title: "Array",
                            url: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                            description: "Javascript Array",
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = `partial prefix word`; MDN only",
                SuggestionQuery {
                    keyword: "wild".into(),
                    providers: vec![SuggestionProvider::Mdn],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = single word; MDN only",
                SuggestionQuery {
                    keyword: "wildcard".into(),
                    providers: vec![SuggestionProvider::Mdn],
                    limit: None,
                },
                expect![[r#"
                    [
                        Mdn {
                            title: "Array",
                            url: "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                            description: "Javascript Array",
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
        ];

        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    #[test]
    fn query_no_yelp_icon_data() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "yelp-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([
                {
                    "subjects": ["ramen"],
                    "preModifiers": [],
                    "postModifiers": [],
                    "locationSigns": [],
                    "yelpModifiers": [],
                    "icon": "yelp-favicon",
                    "score": 0.5
                },
            ]),
        )?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));

        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [(
            "keyword = ramen; Yelp only",
            SuggestionQuery {
                keyword: "ramen".into(),
                providers: vec![SuggestionProvider::Yelp],
                limit: None,
            },
            expect![[r#"
                [
                    Yelp {
                        url: "https://www.yelp.com/search?find_desc=ramen",
                        title: "ramen",
                        icon: None,
                        icon_mimetype: None,
                        score: 0.5,
                        has_location_sign: false,
                        subject_exact_match: true,
                        location_param: "find_loc",
                    },
                ]
            "#]],
        )];

        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        Ok(())
    }

    #[test]
    fn weather() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "weather",
            "last_modified": 15,
            "weather": {
                "min_keyword_length": 3,
                "keywords": ["ab", "xyz", "weather"],
                "score": "0.24"
            }
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        let table = [
            (
                "keyword = 'ab'; Weather only, no match since query is too short",
                SuggestionQuery {
                    keyword: "ab".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xab'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "xab".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'abx'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "abx".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xy'; Weather only, no match since query is too short",
                SuggestionQuery {
                    keyword: "xy".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xyz'; Weather only, match",
                SuggestionQuery {
                    keyword: "xyz".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'xxyz'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "xxyz".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xyzx'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "xyzx".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'we'; Weather only, no match since query is too short",
                SuggestionQuery {
                    keyword: "we".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'wea'; Weather only, match",
                SuggestionQuery {
                    keyword: "wea".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'weat'; Weather only, match",
                SuggestionQuery {
                    keyword: "weat".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'weath'; Weather only, match",
                SuggestionQuery {
                    keyword: "weath".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'weathe'; Weather only, match",
                SuggestionQuery {
                    keyword: "weathe".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'weather'; Weather only, match",
                SuggestionQuery {
                    keyword: "weather".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'weatherx'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "weatherx".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xweather'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "xweather".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = 'xwea'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "xwea".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = '   weather  '; Weather only, match",
                SuggestionQuery {
                    keyword: "   weather  ".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    [
                        Weather {
                            score: 0.24,
                        },
                    ]
                "#]],
            ),
            (
                "keyword = 'x   weather  '; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "x   weather  ".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
            (
                "keyword = '   weather  x'; Weather only, no matching keyword",
                SuggestionQuery {
                    keyword: "   weather  x".into(),
                    providers: vec![SuggestionProvider::Weather],
                    limit: None,
                },
                expect![[r#"
                    []
                "#]],
            ),
        ];

        for (what, query, expect) in table {
            expect.assert_debug_eq(
                &store
                    .query(query)
                    .with_context(|| format!("Couldn't query store for {}", what))?,
            );
        }

        expect![[r#"
            Some(
                Weather {
                    min_keyword_length: 3,
                },
            )
        "#]]
        .assert_debug_eq(
            &store
                .fetch_provider_config(SuggestionProvider::Weather)
                .with_context(|| "Couldn't fetch provider config")?,
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "configuration",
            "last_modified": 15,
            "configuration": {
                "show_less_frequently_cap": 3,
            }
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        expect![[r#"
            SuggestGlobalConfig {
                show_less_frequently_cap: 3,
            }
        "#]]
        .assert_debug_eq(
            &store
                .fetch_global_config()
                .with_context(|| "fetch_global_config failed")?,
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config_default() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([]))?;
        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        expect![[r#"
            SuggestGlobalConfig {
                show_less_frequently_cap: 0,
            }
        "#]]
        .assert_debug_eq(
            &store
                .fetch_global_config()
                .with_context(|| "fetch_global_config failed")?,
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_none() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([]))?;
        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        expect![[r#"
            None
        "#]]
        .assert_debug_eq(
            &store
                .fetch_provider_config(SuggestionProvider::Amp)
                .with_context(|| "fetch_provider_config failed for Amp")?,
        );

        expect![[r#"
            None
        "#]]
        .assert_debug_eq(
            &store
                .fetch_provider_config(SuggestionProvider::Weather)
                .with_context(|| "fetch_provider_config failed for Weather")?,
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_other() -> anyhow::Result<()> {
        before_each();

        // Add some weather config.
        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "weather",
            "last_modified": 15,
            "weather": {
                "min_keyword_length": 3,
                "keywords": ["weather"],
                "score": "0.24"
            }
        }]))?;

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        // Getting the config for a different provider should return None.
        expect![[r#"
            None
        "#]]
        .assert_debug_eq(
            &store
                .fetch_provider_config(SuggestionProvider::Amp)
                .with_context(|| "fetch_provider_config failed for Amp")?,
        );

        Ok(())
    }

    #[test]
    fn remove_dismissed_suggestions() -> anyhow::Result<()> {
        before_each();

        let snapshot = Snapshot::with_records(json!([{
            "id": "data-1",
            "type": "data",
            "last_modified": 15,
            "attachment": {
                "filename": "data-1.json",
                "mimetype": "application/json",
                "location": "data-1.json",
                "hash": "",
                "size": 0,
            },

        }, {
            "id": "data-2",
            "type": "amo-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-2.json",
                "mimetype": "application/json",
                "location": "data-2.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-3",
            "type": "pocket-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-3.json",
                "mimetype": "application/json",
                "location": "data-3.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-5",
            "type": "mdn-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-5.json",
                "mimetype": "application/json",
                "location": "data-5.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "data-6",
            "type": "amp-mobile-suggestions",
            "last_modified": 15,
            "attachment": {
                "filename": "data-6.json",
                "mimetype": "application/json",
                "location": "data-6.json",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-2",
            "type": "icon",
            "last_modified": 20,
            "attachment": {
                "filename": "icon-2.png",
                "mimetype": "image/png",
                "location": "icon-2.png",
                "hash": "",
                "size": 0,
            },
        }, {
            "id": "icon-3",
            "type": "icon",
            "last_modified": 25,
            "attachment": {
                "filename": "icon-3.png",
                "mimetype": "image/png",
                "location": "icon-3.png",
                "hash": "",
                "size": 0,
            },
        }]))?
        .with_data(
            "data-1.json",
            json!([{
                "id": 0,
                "advertiser": "Good Place Eats",
                "iab_category": "8 - Food & Drink",
                "keywords": ["cats"],
                "title": "Lasagna Come Out Tomorrow",
                "url": "https://www.lasagna.restaurant",
                "icon": "2",
                "impression_url": "https://example.com/impression_url",
                "click_url": "https://example.com/click_url",
                "score": 0.31
            }, {
                "id": 0,
                "advertiser": "Wikipedia",
                "iab_category": "5 - Education",
                "keywords": ["cats"],
                "title": "California",
                "url": "https://wikipedia.org/California",
                "icon": "3"
            }]),
        )?
            .with_data(
                "data-2.json",
                json!([
                    {
                        "description": "amo suggestion",
                        "url": "https://addons.mozilla.org/en-US/firefox/addon/example",
                        "guid": "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}",
                        "keywords": ["cats"],
                        "title": "Firefox Relay",
                        "icon": "https://addons.mozilla.org/user-media/addon_icons/2633/2633704-64.png?modified=2c11a80b",
                        "rating": "4.9",
                        "number_of_ratings": 888,
                        "score": 0.32
                    },
                ]),
        )?
            .with_data(
            "data-3.json",
            json!([
                {
                    "description": "pocket suggestion",
                    "url": "https://getpocket.com/collections/its-not-just-burnout-how-grind-culture-failed-women",
                    "lowConfidenceKeywords": [],
                    "highConfidenceKeywords": ["cats"],
                    "title": "‘It’s Not Just Burnout:’ How Grind Culture Fails Women",
                    "score": 0.33
                },
            ]),
        )?
        .with_data(
            "data-5.json",
            json!([
                {
                    "description": "Javascript Array",
                    "url": "https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array",
                    "keywords": ["cats"],
                    "title": "Array",
                    "score": 0.24
                },
            ]),
        )?
        .with_data(
            "data-6.json",
            json!([
               {
                   "id": 0,
                   "advertiser": "Good Place Eats",
                   "iab_category": "8 - Food & Drink",
                   "keywords": ["cats"],
                   "title": "Mobile - Lasagna Come Out Tomorrow",
                   "url": "https://www.lasagna.restaurant",
                   "icon": "3",
                   "impression_url": "https://example.com/impression_url",
                   "click_url": "https://example.com/click_url",
                   "score": 0.26
               }
            ]),
        )?
        .with_icon("icon-2.png", "i-am-an-icon".as_bytes().into())
        .with_icon("icon-3.png", "also-an-icon".as_bytes().into());

        let store = unique_test_store(SnapshotSettingsClient::with_snapshot(snapshot));
        store.ingest(SuggestIngestionConstraints::default())?;

        // A query for cats should return all suggestions
        let query = SuggestionQuery {
            keyword: "cats".into(),
            providers: vec![
                SuggestionProvider::Amp,
                SuggestionProvider::Wikipedia,
                SuggestionProvider::Amo,
                SuggestionProvider::Pocket,
                SuggestionProvider::Mdn,
                SuggestionProvider::AmpMobile,
            ],
            limit: None,
        };
        let results = store.query(query.clone())?;
        assert_eq!(results.len(), 6);

        for result in results {
            store.dismiss_suggestion(result.raw_url().unwrap().to_string())?;
        }

        // After dismissing the suggestions, the next query shouldn't return them
        assert_eq!(store.query(query.clone())?.len(), 0);

        // Clearing the dismissals should cause them to be returned again
        store.clear_dismissed_suggestions()?;
        assert_eq!(store.query(query.clone())?.len(), 6);

        Ok(())
    }
}
