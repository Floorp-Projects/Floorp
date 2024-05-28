/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use std::{
    collections::BTreeSet,
    path::{Path, PathBuf},
    sync::Arc,
};

use error_support::handle_error;
use once_cell::sync::OnceCell;
use parking_lot::Mutex;
use remote_settings::{self, RemoteSettingsConfig, RemoteSettingsServer};

use serde::de::DeserializeOwned;

use crate::{
    config::{SuggestGlobalConfig, SuggestProviderConfig},
    db::{ConnectionType, SuggestDao, SuggestDb},
    error::Error,
    provider::SuggestionProvider,
    rs::{
        Client, Record, RecordRequest, SuggestAttachment, SuggestRecord, SuggestRecordId,
        SuggestRecordType, DEFAULT_RECORDS_TYPES, REMOTE_SETTINGS_COLLECTION,
    },
    Result, SuggestApiResult, Suggestion, SuggestionQuery,
};

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

/// What should be interrupted when [SuggestStore::interrupt] is called?
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum InterruptKind {
    /// Interrupt read operations like [SuggestStore::query]
    Read,
    /// Interrupt write operations.  This mostly means [SuggestStore::ingest], but
    /// [SuggestStore::dismiss_suggestion] may also be interrupted.
    Write,
    /// Interrupt both read and write operations,
    ReadWrite,
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
    pub fn interrupt(&self, kind: Option<InterruptKind>) {
        self.inner.interrupt(kind)
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
    /// Only run ingestion if the table `suggestions` is empty
    pub empty_only: bool,
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

    fn interrupt(&self, kind: Option<InterruptKind>) {
        if let Some(dbs) = self.dbs.get() {
            // Only interrupt if the databases are already open.
            match kind.unwrap_or(InterruptKind::Read) {
                InterruptKind::Read => {
                    dbs.reader.interrupt_handle.interrupt();
                }
                InterruptKind::Write => {
                    dbs.writer.interrupt_handle.interrupt();
                }
                InterruptKind::ReadWrite => {
                    dbs.reader.interrupt_handle.interrupt();
                    dbs.writer.interrupt_handle.interrupt();
                }
            }
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
    S: Client,
{
    pub fn ingest(&self, constraints: SuggestIngestionConstraints) -> Result<()> {
        let writer = &self.dbs()?.writer;
        if constraints.empty_only && !writer.read(|dao| dao.suggestions_table_empty())? {
            return Ok(());
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

        // Handle ingestion inside single write scope
        let mut write_scope = writer.write_scope()?;
        for ingest_record_type in ingest_record_types {
            write_scope
                .write(|dao| self.ingest_records_by_type(ingest_record_type, dao, &constraints))?;
            write_scope.err_if_interrupted()?;
        }

        Ok(())
    }

    fn ingest_records_by_type(
        &self,
        ingest_record_type: SuggestRecordType,
        dao: &mut SuggestDao,
        constraints: &SuggestIngestionConstraints,
    ) -> Result<()> {
        let request = RecordRequest {
            record_type: Some(ingest_record_type.to_string()),
            last_modified: dao
                .get_meta::<u64>(ingest_record_type.last_ingest_meta_key().as_str())?,
            limit: constraints.max_suggestions,
        };

        let records = self.settings_client.get_records(request)?;
        self.ingest_records(&ingest_record_type.last_ingest_meta_key(), dao, &records)?;
        Ok(())
    }

    fn ingest_records(
        &self,
        last_ingest_key: &str,
        dao: &mut SuggestDao,
        records: &[Record],
    ) -> Result<()> {
        for record in records {
            let record_id = SuggestRecordId::from(&record.id);
            if record.deleted {
                // If the entire record was deleted, drop all its suggestions
                // and advance the last ingest time.
                dao.handle_deleted_record(last_ingest_key, record)?;
                continue;
            }
            let Ok(fields) =
                serde_json::from_value(serde_json::Value::Object(record.fields.clone()))
            else {
                // We don't recognize this record's type, so we don't know how
                // to ingest its suggestions. Skip processing this record.
                continue;
            };

            match fields {
                SuggestRecord::AmpWikipedia => {
                    self.ingest_attachment(
                        // TODO: Currently re-creating the last_ingest_key because using last_ingest_meta
                        // breaks the tests (particularly the unparsable functionality). So, keeping
                        // a direct reference until we remove the "unparsable" functionality.
                        &SuggestRecordType::AmpWikipedia.last_ingest_meta_key(),
                        dao,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amp_wikipedia_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::AmpMobile => {
                    self.ingest_attachment(
                        &SuggestRecordType::AmpMobile.last_ingest_meta_key(),
                        dao,
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
                        dao.put_last_ingest_if_newer(
                            &SuggestRecordType::Icon.last_ingest_meta_key(),
                            record.last_modified,
                        )?;
                        continue;
                    };
                    let data = record.require_attachment_data()?;
                    dao.put_icon(icon_id, data, &attachment.mimetype)?;
                    dao.handle_ingested_record(
                        &SuggestRecordType::Icon.last_ingest_meta_key(),
                        record,
                    )?;
                }
                SuggestRecord::Amo => {
                    self.ingest_attachment(
                        &SuggestRecordType::Amo.last_ingest_meta_key(),
                        dao,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_amo_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Pocket => {
                    self.ingest_attachment(
                        &SuggestRecordType::Pocket.last_ingest_meta_key(),
                        dao,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_pocket_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Yelp => {
                    self.ingest_attachment(
                        &SuggestRecordType::Yelp.last_ingest_meta_key(),
                        dao,
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
                        dao,
                        record,
                        |dao, record_id, suggestions| {
                            dao.insert_mdn_suggestions(record_id, suggestions)
                        },
                    )?;
                }
                SuggestRecord::Weather(data) => {
                    self.ingest_record(
                        &SuggestRecordType::Weather.last_ingest_meta_key(),
                        dao,
                        record,
                        |dao, record_id| dao.insert_weather_data(record_id, &data),
                    )?;
                }
                SuggestRecord::GlobalConfig(config) => {
                    self.ingest_record(
                        &SuggestRecordType::GlobalConfig.last_ingest_meta_key(),
                        dao,
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
        dao: &mut SuggestDao,
        record: &Record,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId) -> Result<()>,
    ) -> Result<()> {
        let record_id = SuggestRecordId::from(&record.id);

        // Drop any data that we previously ingested from this record.
        // Suggestions in particular don't have a stable identifier, and
        // determining which suggestions in the record actually changed is
        // more complicated than dropping and re-ingesting all of them.
        dao.drop_suggestions(&record_id)?;

        // Ingest (or re-ingest) all data in the record.
        ingestion_handler(dao, &record_id)?;

        dao.handle_ingested_record(last_ingest_key, record)
    }

    fn ingest_attachment<T>(
        &self,
        last_ingest_key: &str,
        dao: &mut SuggestDao,
        record: &Record,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId, &[T]) -> Result<()>,
    ) -> Result<()>
    where
        T: DeserializeOwned,
    {
        if record.attachment.is_none() {
            // This method should be called only when a record is expected to
            // have an attachment. If it doesn't have one, it's malformed, so
            // skip to the next record.
            dao.put_last_ingest_if_newer(last_ingest_key, record.last_modified)?;
            return Ok(());
        };

        let attachment_data = record.require_attachment_data()?;
        match serde_json::from_slice::<SuggestAttachment<T>>(attachment_data) {
            Ok(attachment) => self.ingest_record(last_ingest_key, dao, record, |dao, record_id| {
                ingestion_handler(dao, record_id, attachment.suggestions())
            }),
            // If the attachment doesn't match our expected schema, just skip it.  It's possible
            // that we're using an older version.  If so, we'll get the data when we re-ingest
            // after updating the schema.
            Err(_) => Ok(()),
        }
    }
}

#[cfg(feature = "benchmark_api")]
impl<S> SuggestStoreInner<S>
where
    S: Client,
{
    pub fn into_settings_client(self) -> S {
        self.settings_client
    }

    pub fn ensure_db_initialized(&self) {
        self.dbs().unwrap();
    }

    pub fn benchmark_ingest_records_by_type(&self, ingest_record_type: SuggestRecordType) {
        let writer = &self.dbs().unwrap().writer;
        writer
            .write(|dao| {
                self.ingest_records_by_type(
                    ingest_record_type,
                    dao,
                    &SuggestIngestionConstraints::default(),
                )
            })
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

    use std::sync::atomic::{AtomicUsize, Ordering};

    use parking_lot::Once;
    use serde_json::json;
    use sql_support::ConnExt;

    use crate::{testing::*, SuggestionProvider};

    /// In-memory Suggest store for testing
    struct TestStore {
        pub inner: SuggestStoreInner<MockRemoteSettingsClient>,
    }

    impl TestStore {
        fn new(client: MockRemoteSettingsClient) -> Self {
            static COUNTER: AtomicUsize = AtomicUsize::new(0);
            let db_path = format!(
                "file:test_store_data_{}?mode=memory&cache=shared",
                COUNTER.fetch_add(1, Ordering::Relaxed),
            );
            Self {
                inner: SuggestStoreInner::new(db_path, client),
            }
        }

        fn replace_client(&mut self, client: MockRemoteSettingsClient) {
            self.inner.settings_client = client;
        }

        fn last_modified_timestamp(&self) -> u64 {
            self.inner.settings_client.last_modified_timestamp
        }

        fn read<T>(&self, op: impl FnOnce(&SuggestDao) -> Result<T>) -> Result<T> {
            self.inner.dbs().unwrap().reader.read(op)
        }

        fn write<T>(&self, op: impl FnOnce(&mut SuggestDao) -> Result<T>) -> Result<T> {
            self.inner.dbs().unwrap().writer.write(op)
        }

        fn count_rows(&self, table_name: &str) -> u64 {
            let sql = format!("SELECT count(*) FROM {table_name}");
            self.read(|dao| Ok(dao.conn.query_one(&sql)?))
                .unwrap_or_else(|e| panic!("SQL error in count: {e}"))
        }

        fn ingest(&self, constraints: SuggestIngestionConstraints) {
            self.inner.ingest(constraints).unwrap();
        }

        fn fetch_suggestions(&self, query: SuggestionQuery) -> Vec<Suggestion> {
            self.inner
                .dbs()
                .unwrap()
                .reader
                .read(|dao| Ok(dao.fetch_suggestions(&query).unwrap()))
                .unwrap()
        }

        pub fn fetch_global_config(&self) -> SuggestGlobalConfig {
            self.inner
                .fetch_global_config()
                .expect("Error fetching global config")
        }

        pub fn fetch_provider_config(
            &self,
            provider: SuggestionProvider,
        ) -> Option<SuggestProviderConfig> {
            self.inner
                .fetch_provider_config(provider)
                .expect("Error fetching provider config")
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

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "1234", json![los_pollos_amp()])
                .with_icon(los_pollos_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );
        Ok(())
    }

    /// Tests ingesting suggestions into an empty database.
    #[test]
    fn ingest_empty_only() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json![los_pollos_amp()],
        ));
        // suggestions_table_empty returns true before the ingestion is complete
        assert!(store.read(|dao| dao.suggestions_table_empty())?);
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints {
            empty_only: true,
            ..SuggestIngestionConstraints::default()
        });
        // suggestions_table_empty returns false after the ingestion is complete
        assert!(!store.read(|dao| dao.suggestions_table_empty())?);

        // This ingestion should not run since the DB is no longer empty
        store.replace_client(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json!([los_pollos_amp(), good_place_eats_amp()]),
        ));
        store.ingest(SuggestIngestionConstraints {
            empty_only: true,
            ..SuggestIngestionConstraints::default()
        });
        // "la" should not match the good place eats suggestion, since that should not have been
        // ingested.
        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("la")), vec![]);

        Ok(())
    }

    /// Tests ingesting suggestions with icons.
    #[test]
    fn ingest_amp_icons() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "1234",
                    json!([los_pollos_amp(), good_place_eats_amp()]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")]
        );

        Ok(())
    }

    #[test]
    fn ingest_full_keywords() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default()
            .with_record("data", "1234", json!([
                // AMP attachment with full keyword data
                los_pollos_amp().merge(json!({
                    "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                    "full_keywords": [
                        // Full keyword for the first 4 keywords
                        ("los pollos", 4),
                        // Full keyword for the next 2 keywords
                        ("los pollos hermanos (restaurant)", 2),
                    ],
                })),
                // AMP attachment without full keyword data
                good_place_eats_amp(),
                // Wikipedia attachment with full keyword data.  We should ignore the full
                // keyword data for Wikipedia suggestions
                california_wiki(),
                // california_wiki().merge(json!({
                //     "keywords": ["cal", "cali", "california"],
                //     "full_keywords": [("california institute of technology", 3)],
                // })),
            ]))
            .with_record("amp-mobile-suggestions", "2468", json!([
                // Amp mobile attachment with full keyword data
                a1a_amp_mobile().merge(json!({
                    "keywords": ["a1a", "ca", "car", "car wash"],
                    "full_keywords": [
                        ("A1A Car Wash", 1),
                        ("car wash", 3),
                    ],
                })),
            ]))
            .with_icon(los_pollos_icon())
            .with_icon(good_place_eats_icon())
            .with_icon(california_icon())
        );
        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            // This keyword comes from the provided full_keywords list
            vec![los_pollos_suggestion("los pollos")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            // Good place eats did not have full keywords, so this one is calculated with the
            // keywords.rs code
            vec![good_place_eats_suggestion("lasagna")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal")),
            // Even though this had a full_keywords field, we should ignore it since it's a
            // wikipedia suggestion and use the keywords.rs code instead
            vec![california_suggestion("california")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp_mobile("a1a")),
            // This keyword comes from the provided full_keywords list.
            vec![a1a_suggestion("A1A Car Wash")],
        );

        Ok(())
    }

    /// Tests ingesting a data attachment containing a single suggestion,
    /// instead of an array of suggestions.
    #[test]
    fn ingest_one_suggestion_in_data_attachment() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // This record contains just one JSON object, rather than an array of them
                .with_record("data", "1234", los_pollos_amp())
                .with_icon(los_pollos_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );

        Ok(())
    }

    /// Tests re-ingesting suggestions from an updated attachment.
    #[test]
    fn reingest_amp_suggestions() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json!([los_pollos_amp(), good_place_eats_amp()]),
        ));
        // Ingest once
        store.ingest(SuggestIngestionConstraints::default());
        // Update the snapshot with new suggestions: Los pollos has a new name and Good place eats
        // is now serving Penne
        store.replace_client(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json!([
                los_pollos_amp().merge(json!({
                    "title": "Los Pollos Hermanos - Now Serving at 14 Locations!",
                })),
                good_place_eats_amp().merge(json!({
                    "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                    "title": "Penne for Your Thoughts",
                    "url": "https://penne.biz",
                }))
            ]),
        ));
        store.ingest(SuggestIngestionConstraints::default());

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")).as_slice(),
            [ Suggestion::Amp { title, .. } ] if title == "Los Pollos Hermanos - Now Serving at 14 Locations!",
        ));

        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("la")), vec![]);
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("pe")).as_slice(),
            [ Suggestion::Amp { title, url, .. } ] if title == "Penne for Your Thoughts" && url == "https://penne.biz"
        ));

        Ok(())
    }

    /// Tests re-ingesting icons from an updated attachment.
    #[test]
    fn reingest_icons() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "1234",
                    json!([los_pollos_amp(), good_place_eats_amp()]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints::default());

        // Reingest with updated icon data
        //  - Los pollos gets new data and a new id
        //  - Good place eats gets new data only
        store.replace_client(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "1234",
                    json!([
                        los_pollos_amp().merge(json!({"icon": "1000"})),
                        good_place_eats_amp()
                    ]),
                )
                .with_icon(MockIcon {
                    id: "1000",
                    data: "new-los-pollos-icon",
                    ..los_pollos_icon()
                })
                .with_icon(MockIcon {
                    data: "new-good-place-eats-icon",
                    ..good_place_eats_icon()
                }),
        );
        store.ingest(SuggestIngestionConstraints::default());

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")).as_slice(),
            [ Suggestion::Amp { icon, .. } ] if *icon == Some("new-los-pollos-icon".as_bytes().to_vec())
        ));

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("la")).as_slice(),
            [ Suggestion::Amp { icon, .. } ] if *icon == Some("new-good-place-eats-icon".as_bytes().to_vec())
        ));

        Ok(())
    }

    /// Tests re-ingesting AMO suggestions from an updated attachment.
    #[test]
    fn reingest_amo_suggestions() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("amo-suggestions", "data-1", json!([relay_amo()]))
                .with_record(
                    "amo-suggestions",
                    "data-2",
                    json!([dark_mode_amo(), foxy_guestures_amo()]),
                ),
        );

        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("night")),
            vec![dark_mode_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("grammar")),
            vec![foxy_guestures_suggestion()],
        );

        // Update the snapshot with new suggestions: update the second, drop the
        // third, and add the fourth.
        store.replace_client(
            MockRemoteSettingsClient::default()
                .with_record("amo-suggestions", "data-1", json!([relay_amo()]))
                .with_record(
                    "amo-suggestions",
                    "data-2",
                    json!([
                        dark_mode_amo().merge(json!({"title": "Updated second suggestion"})),
                        new_tab_override_amo(),
                    ]),
                ),
        );
        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amo("night")).as_slice(),
            [Suggestion::Amo { title, .. } ] if title == "Updated second suggestion"
        ));
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("grammar")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("image search")),
            vec![new_tab_override_suggestion()],
        );

        Ok(())
    }

    /// Tests ingesting tombstones for previously-ingested suggestions and
    /// icons.
    #[test]
    fn ingest_tombstones() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("data", "data-2", json!([good_place_eats_amp()]))
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")],
        );
        // Re-ingest with:
        //   - Los pollos replaced with a tombstone
        //   - Good place eat's icon replaced with a tombstone
        store.replace_client(
            MockRemoteSettingsClient::default()
                .with_tombstone("data", "data-1")
                .with_record("data", "data-2", json!([good_place_eats_amp()]))
                .with_icon_tombstone(los_pollos_icon())
                .with_icon_tombstone(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("lo")), vec![]);
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("la")).as_slice(),
            [
                Suggestion::Amp { icon, icon_mimetype, .. }
            ] if icon.is_none() && icon_mimetype.is_none(),
        ));
        Ok(())
    }

    /// Tests clearing the store.
    #[test]
    fn clear() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("data", "data-2", json!([good_place_eats_amp()]))
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());
        assert!(store.count_rows("suggestions") > 0);
        assert!(store.count_rows("keywords") > 0);
        assert!(store.count_rows("icons") > 0);

        store.inner.clear()?;
        assert!(store.count_rows("suggestions") == 0);
        assert!(store.count_rows("keywords") == 0);
        assert!(store.count_rows("icons") == 0);

        Ok(())
    }

    /// Tests querying suggestions.
    #[test]
    fn query() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        good_place_eats_amp(),
                        california_wiki(),
                        caltech_wiki(),
                        multimatch_wiki(),
                    ]),
                )
                .with_record(
                    "amo-suggestions",
                    "data-2",
                    json!([relay_amo(), multimatch_amo(),]),
                )
                .with_record(
                    "pocket-suggestions",
                    "data-3",
                    json!([burnout_pocket(), multimatch_pocket(),]),
                )
                .with_record("yelp-suggestions", "data-4", json!([ramen_yelp(),]))
                .with_record("yeld-suggestions", "data-4", json!([ramen_yelp(),]))
                .with_record("mdn-suggestions", "data-5", json!([array_mdn(),]))
                .with_icon(good_place_eats_icon())
                .with_icon(california_icon())
                .with_icon(caltech_icon())
                .with_icon(yelp_favicon())
                .with_icon(multimatch_wiki_icon()),
        );

        store.ingest(SuggestIngestionConstraints::default());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("la")),
            vec![good_place_eats_suggestion("lasagna"),]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("multimatch")),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
                multimatch_wiki_suggestion(),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("MultiMatch")),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
                multimatch_wiki_suggestion(),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("multimatch").limit(2)),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers_except(
                "la",
                SuggestionProvider::Amp
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers("la", vec![])),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "cal",
                vec![
                    SuggestionProvider::Amp,
                    SuggestionProvider::Amo,
                    SuggestionProvider::Pocket,
                ]
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal")),
            vec![
                california_suggestion("california"),
                caltech_suggestion("california"),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal").limit(1)),
            vec![california_suggestion("california"),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers("cal", vec![])),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("spam")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking s")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "soft",
                vec![SuggestionProvider::Amp, SuggestionProvider::Wikipedia]
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("soft")),
            vec![burnout_suggestion(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("soft l")),
            vec![burnout_suggestion(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("sof")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("burnout women")),
            vec![burnout_suggestion(true),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("burnout person")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best spicy ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "best spicy ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=best+spicy+ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt SpIcY rAmEn DeLiVeRy In ToKyO")),
            vec![ramen_suggestion(
                "BeSt SpIcY rAmEn DeLiVeRy In ToKyO",
                "https://www.yelp.com/search?find_desc=BeSt+SpIcY+rAmEn+DeLiVeRy&find_loc=ToKyO"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "best ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=best+ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "best invalid_ramen delivery in tokyo"
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("super best ramen in tokyo")),
            vec![ramen_suggestion(
                "super best ramen in tokyo",
                "https://www.yelp.com/search?find_desc=super+best+ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("invalid_best ramen in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen super delivery in tokyo")),
            vec![ramen_suggestion(
                "ramen super delivery in tokyo",
                "https://www.yelp.com/search?find_desc=ramen+super+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen invalid_delivery in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near tokyo")),
            vec![ramen_suggestion(
                "ramen near tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen invalid_in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in San Francisco")),
            vec![ramen_suggestion(
                "ramen in San Francisco",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=San+Francisco"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in")),
            vec![ramen_suggestion(
                "ramen in",
                "https://www.yelp.com/search?find_desc=ramen"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near by")),
            vec![ramen_suggestion(
                "ramen near by",
                "https://www.yelp.com/search?find_desc=ramen+near+by"
            )
            .has_location_sign(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near me")),
            vec![ramen_suggestion(
                "ramen near me",
                "https://www.yelp.com/search?find_desc=ramen+near+me"
            )
            .has_location_sign(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near by tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        // Test an extremely long yelp query
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
            )),
            vec![
                ramen_suggestion(
                    "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789",
                    "https://www.yelp.com/search?find_desc=012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
                ).has_location_sign(false),
            ],
        );
        // This query is over the limit and no suggestions should be returned
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789Z"
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best delivery")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("same_modifier same_modifier")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("same_modifier ")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp keyword ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo yelp")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            )],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo yelp keyword")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            )],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp ramen yelp")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best yelp ramen")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("Spicy R")),
            vec![ramen_suggestion(
                "Spicy Ramen",
                "https://www.yelp.com/search?find_desc=Spicy+Ramen"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             Ramen")),
            vec![ramen_suggestion(
                "BeSt Ramen",
                "https://www.yelp.com/search?find_desc=BeSt+Ramen"
            )
            .has_location_sign(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             Spicy R")),
            vec![ramen_suggestion(
                "BeSt Spicy Ramen",
                "https://www.yelp.com/search?find_desc=BeSt+Spicy+Ramen"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             R")),
            vec![],
        );
        assert_eq!(store.fetch_suggestions(SuggestionQuery::yelp("r")), vec![],);
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ra")),
            vec![
                ramen_suggestion("rats", "https://www.yelp.com/search?find_desc=rats")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ram")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("rac")),
            vec![
                ramen_suggestion("raccoon", "https://www.yelp.com/search?find_desc=raccoon")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best r")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ra")),
            vec![ramen_suggestion(
                "best rats",
                "https://www.yelp.com/search?find_desc=best+rats"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );

        Ok(())
    }

    // Tests querying AMP / Wikipedia / Pocket
    #[test]
    fn query_with_multiple_providers_and_diff_scores() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            // Create a data set where one keyword matches multiple suggestions from each provider
            // where the scores are manually set.  We will test that the fetched suggestions are in
            // the correct order.
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        los_pollos_amp().merge(json!({
                            "keywords": ["amp wiki match"],
                            "score": 0.3,
                        })),
                        good_place_eats_amp().merge(json!({
                            "keywords": ["amp wiki match"],
                            "score": 0.1,
                        })),
                        california_wiki().merge(json!({
                            "keywords": ["amp wiki match", "pocket wiki match"],
                        })),
                    ]),
                )
                .with_record(
                    "pocket-suggestions",
                    "data-3",
                    json!([
                        burnout_pocket().merge(json!({
                            "lowConfidenceKeywords": ["work-life balance", "pocket wiki match"],
                            "score": 0.05,
                        })),
                        multimatch_pocket().merge(json!({
                            "highConfidenceKeywords": ["pocket wiki match"],
                            "score": 0.88,
                        })),
                    ]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon())
                .with_icon(california_icon()),
        );

        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("amp wiki match")),
            vec![
                los_pollos_suggestion("amp wiki match").with_score(0.3),
                // Wikipedia entries default to a 0.2 score
                california_suggestion("amp wiki match"),
                good_place_eats_suggestion("amp wiki match").with_score(0.1),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("amp wiki match").limit(2)),
            vec![
                los_pollos_suggestion("amp wiki match").with_score(0.3),
                california_suggestion("amp wiki match"),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("pocket wiki match")),
            vec![
                multimatch_pocket_suggestion(true).with_score(0.88),
                california_suggestion("pocket wiki match"),
                burnout_suggestion(false).with_score(0.05),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("pocket wiki match").limit(1)),
            vec![multimatch_pocket_suggestion(true).with_score(0.88),]
        );
        // test duplicate providers
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "work-life balance",
                vec![SuggestionProvider::Pocket, SuggestionProvider::Pocket],
            )),
            vec![burnout_suggestion(false).with_score(0.05),]
        );

        Ok(())
    }

    // Tests querying multiple suggestions with multiple keywords with same prefix keyword
    #[test]
    fn query_with_amp_mobile_provider() -> anyhow::Result<()> {
        before_each();

        // Use the exact same data for both the Amp and AmpMobile record
        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "amp-mobile-suggestions",
                    "amp-mobile-1",
                    json!([good_place_eats_amp()]),
                )
                .with_record("data", "data-1", json!([good_place_eats_amp()]))
                // This icon is shared by both records which is kind of weird and probably not how
                // things would work in practice, but it's okay for the tests.
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());
        // The query results should be exactly the same for both the Amp and AmpMobile data
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp_mobile("las")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("las")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        Ok(())
    }

    /// Tests ingesting malformed Remote Settings records that we understand,
    /// but that are missing fields, or aren't in the format we expect.
    #[test]
    fn ingest_malformed() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // Amp/Wikipedia record without an attachment.
                .with_record_but_no_attachment("data", "data-1")
                // Icon record without an attachment.
                .with_record_but_no_attachment("icon", "icon-1")
                // Icon record with an ID that's not `icon-{id}`, so suggestions in
                // the data attachment won't be able to reference it.
                .with_record("icon", "bad-icon-id", json!("i-am-an-icon")),
        );

        store.ingest(SuggestIngestionConstraints::default());

        store.read(|dao| {
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(store.last_modified_timestamp())
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

    /// Tests that we only ingest providers that we're concerned with.
    #[test]
    fn ingest_constraints_provider() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("yelp", "yelp-1", json!([ramen_yelp()]))
                .with_icon(los_pollos_icon()),
        );

        // Write a last ingestion times to test that we overwrite it properly
        store.write(|dao| {
            // Check that existing data is updated properly.
            dao.put_meta(
                SuggestRecordType::AmpWikipedia
                    .last_ingest_meta_key()
                    .as_str(),
                1,
            )?;
            Ok(())
        })?;

        let constraints = SuggestIngestionConstraints {
            max_suggestions: Some(100),
            providers: Some(vec![SuggestionProvider::Amp, SuggestionProvider::Pocket]),
            ..SuggestIngestionConstraints::default()
        };
        store.ingest(constraints);

        // This should have been ingested
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")]
        );
        // This should not have been ingested, since it wasn't in the providers list
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ramen")),
            vec![]
        );

        store.read(|dao| {
            // This should have its last_modified_timestamp updated, since we ingested an amp
            // record
            assert_eq!(
                dao.get_meta::<u64>(
                    SuggestRecordType::AmpWikipedia
                        .last_ingest_meta_key()
                        .as_str()
                )?,
                Some(store.last_modified_timestamp())
            );
            // This should have its last_modified_timestamp updated, since we ingested an icon
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Icon.last_ingest_meta_key().as_str())?,
                Some(store.last_modified_timestamp())
            );
            // This should not have its last_modified_timestamp updated, since there were no pocket
            // items to ingest
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Pocket.last_ingest_meta_key().as_str())?,
                None
            );
            // This should not have its last_modified_timestamp updated, since we did not ask to
            // ingest yelp items.
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Yelp.last_ingest_meta_key().as_str())?,
                None
            );
            assert_eq!(
                dao.get_meta::<u64>(SuggestRecordType::Amo.last_ingest_meta_key().as_str())?,
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

    /// Tests that records with invalid attachments are ignored
    #[test]
    fn skip_over_invalid_records() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // valid record
                .with_record("data", "data-1", json!([good_place_eats_amp()]))
                // This attachment is missing the `title` field and is invalid
                .with_record(
                    "data",
                    "data-2",
                    json!([{
                            "id": 1,
                            "advertiser": "Los Pollos Hermanos",
                            "iab_category": "8 - Food & Drink",
                            "keywords": ["lo", "los", "los pollos"],
                            "url": "https://www.lph-nm.biz",
                            "icon": "5678",
                            "impression_url": "https://example.com/impression_url",
                            "click_url": "https://example.com/click_url",
                            "score": 0.3
                    }]),
                )
                .with_icon(good_place_eats_icon()),
        );

        store.ingest(SuggestIngestionConstraints::default());

        // Test that the valid record was read
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        // Test that the invalid record was skipped
        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("lo")), vec![]);

        Ok(())
    }

    #[test]
    fn query_mdn() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "mdn-suggestions",
            "mdn-1",
            json!([array_mdn()]),
        ));
        store.ingest(SuggestIngestionConstraints::default());
        // prefix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("array")),
            vec![array_suggestion(),]
        );
        // prefix + partial suffix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("array java")),
            vec![array_suggestion(),]
        );
        // prefix + entire suffix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("javascript array")),
            vec![array_suggestion(),]
        );
        // partial prefix word
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("wild")),
            vec![]
        );
        // single word
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("wildcard")),
            vec![array_suggestion()]
        );
        Ok(())
    }

    #[test]
    fn query_no_yelp_icon_data() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default().with_record(
                "yelp-suggestions",
                "yelp-1",
                json!([ramen_yelp()]),
            ), // Note: yelp_favicon() is missing
        );
        store.ingest(SuggestIngestionConstraints::default());
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen")).as_slice(),
            [Suggestion::Yelp { icon, icon_mimetype, .. }] if icon.is_none() && icon_mimetype.is_none()
        ));

        Ok(())
    }

    #[test]
    fn weather() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "weather",
            "weather-1",
            json!({
                "min_keyword_length": 3,
                "keywords": ["ab", "xyz", "weather"],
                "score": "0.24"
            }),
        ));
        store.ingest(SuggestIngestionConstraints::default());
        // No match since the query doesn't match any keyword
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xab")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("abx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xxyz")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xyzx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weatherx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xweather")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xwea")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("x   weather")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("   weather x")),
            vec![]
        );
        // No match since the query is too short
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xy")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("ab")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("we")),
            vec![]
        );
        // Matches
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xyz")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("wea")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weat")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weath")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weathe")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weather")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("  weather  ")),
            vec![Suggestion::Weather { score: 0.24 },]
        );

        assert_eq!(
            store.fetch_provider_config(SuggestionProvider::Weather),
            Some(SuggestProviderConfig::Weather {
                min_keyword_length: 3,
            })
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "configuration",
            "configuration-1",
            json!({
                "show_less_frequently_cap": 3,
            }),
        ));
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_global_config(),
            SuggestGlobalConfig {
                show_less_frequently_cap: 3,
            }
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config_default() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default());
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(
            store.fetch_global_config(),
            SuggestGlobalConfig {
                show_less_frequently_cap: 0,
            }
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_none() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default());
        store.ingest(SuggestIngestionConstraints::default());
        assert_eq!(store.fetch_provider_config(SuggestionProvider::Amp), None);
        assert_eq!(
            store.fetch_provider_config(SuggestionProvider::Weather),
            None
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_other() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "weather",
            "weather-1",
            json!({
                "min_keyword_length": 3,
                "keywords": ["weather"],
                "score": "0.24"
            }),
        ));
        store.ingest(SuggestIngestionConstraints::default());
        // Getting the config for a different provider should return None.
        assert_eq!(store.fetch_provider_config(SuggestionProvider::Amp), None);
        Ok(())
    }

    #[test]
    fn remove_dismissed_suggestions() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        good_place_eats_amp().merge(json!({"keywords": ["cats"]})),
                        california_wiki().merge(json!({"keywords": ["cats"]})),
                    ]),
                )
                .with_record(
                    "amo-suggestions",
                    "amo-1",
                    json!([relay_amo().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_record(
                    "pocket-suggestions",
                    "pocket-1",
                    json!([burnout_pocket().merge(json!({
                        "lowConfidenceKeywords": ["cats"],
                    }))]),
                )
                .with_record(
                    "mdn-suggestions",
                    "mdn-1",
                    json!([array_mdn().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_record(
                    "amp-mobile-suggestions",
                    "amp-mobile-1",
                    json!([a1a_amp_mobile().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_icon(good_place_eats_icon())
                .with_icon(caltech_icon()),
        );
        store.ingest(SuggestIngestionConstraints::default());

        // A query for cats should return all suggestions
        let query = SuggestionQuery::all_providers("cats");
        let results = store.fetch_suggestions(query.clone());
        assert_eq!(results.len(), 6);

        for result in results {
            store
                .inner
                .dismiss_suggestion(result.raw_url().unwrap().to_string())?;
        }

        // After dismissing the suggestions, the next query shouldn't return them
        assert_eq!(store.fetch_suggestions(query.clone()).len(), 0);

        // Clearing the dismissals should cause them to be returned again
        store.inner.clear_dismissed_suggestions()?;
        assert_eq!(store.fetch_suggestions(query.clone()).len(), 6);

        Ok(())
    }
}
